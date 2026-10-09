#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>
#include <QThread>

#include <algorithm>

namespace {

QString valueAfter(const QStringList& arguments, const QString& option)
{
    const int index = arguments.indexOf(option);
    return index >= 0 ? arguments.value(index + 1) : QString();
}

QString valueStartingWith(const QStringList& arguments, const QString& prefix)
{
    for (const QString& argument : arguments) {
        if (argument.startsWith(prefix)) {
            return argument.mid(prefix.size());
        }
    }
    return {};
}

int integerAfter(const QStringList& arguments, const QString& option, const int fallback = 0)
{
    bool ok = false;
    const int value = valueAfter(arguments, option).toInt(&ok);
    return ok ? value : fallback;
}

QJsonObject readPlan(const QString& root)
{
    QFile file(QDir(root).filePath(QStringLiteral("fake-plan.json")));
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

void logInvocation(const QString& root, const QString& tool, const QStringList& arguments)
{
    QFile file(QDir(root).filePath(QStringLiteral("invocations.jsonl")));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        return;
    }
    QJsonObject entry;
    entry.insert(QStringLiteral("tool"), tool);
    entry.insert(QStringLiteral("arguments"), QJsonArray::fromStringList(arguments));
    file.write(QJsonDocument(entry).toJson(QJsonDocument::Compact));
    file.write("\n");
}

bool saveImage(const QString& path, const int width, const int height)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImage image(width, height, QImage::Format_RGBA8888);
    image.fill(QColor(36, 99, 235, 255));
    return image.save(path);
}

QSize imageSize(const QString& input)
{
    if (QFileInfo(input).isDir()) {
        const QFileInfoList frames = QDir(input).entryInfoList(
            { QStringLiteral("*.png") }, QDir::Files, QDir::Name);
        return frames.isEmpty() ? QSize(16, 12) : QImage(frames.front().absoluteFilePath()).size();
    }
    const QImage image(input);
    return image.isNull() ? QSize(16, 12) : image.size();
}

bool writeFrames(const QString& output, const int count, const QSize& size,
                 const QJsonObject& plan = {})
{
    const bool pattern = output.contains(QLatin1Char('%'));
    const QString directory = pattern ? QFileInfo(output).absolutePath() : output;
    QDir().mkpath(directory);
    for (int frame = 1; frame <= count; ++frame) {
        QString path;
        if (pattern) {
            path = output;
            const QRegularExpression expression(QStringLiteral("%0?(\\d*)d"));
            const auto match = expression.match(path);
            const int width = match.captured(1).isEmpty() ? 0 : match.captured(1).toInt();
            path.replace(match.capturedStart(), match.capturedLength(),
                         QString::number(frame).rightJustified(width, QLatin1Char('0')));
        } else {
            path = QDir(directory).filePath(
                QStringLiteral("%1.png").arg(frame, 8, 10, QLatin1Char('0')));
        }
        if (plan.value(QStringLiteral("slowFrameWrites")).toBool()) {
            // NCNN can report 100% for one tile while its output frame is
            // incomplete. Keep that file visible across two polling ticks.
            QFile partial(path);
            if (!partial.open(QIODevice::WriteOnly)) {
                return false;
            }
            partial.write(QByteArray::fromHex("89504e470d0a1a0a"));
            partial.close();
            QTextStream stream(stdout);
            stream << "100.00%\n";
            stream.flush();
            QThread::msleep(650);
        }
        if (!saveImage(path, size.width(), size.height())) {
            return false;
        }
        if (plan.value(QStringLiteral("slowFrameWrites")).toBool()) {
            QThread::msleep(400);
        }
    }
    return true;
}

QJsonObject sourceProbe(const QJsonObject& plan)
{
    const int width = plan.value(QStringLiteral("width")).toInt(16);
    const int height = plan.value(QStringLiteral("height")).toInt(12);
    const int frames = plan.value(QStringLiteral("frames")).toInt(4);
    const double fps = plan.value(QStringLiteral("fps")).toDouble(4.0);
    const double defaultDuration = fps > 0.0 ? frames / fps : 0.0;
    const QString sourceFrames = plan.contains(QStringLiteral("sourceFrames"))
        ? (plan.value(QStringLiteral("sourceFrames")).isString()
               ? plan.value(QStringLiteral("sourceFrames")).toString()
               : QString::number(plan.value(QStringLiteral("sourceFrames")).toInt()))
        : QString::number(frames);
    QJsonObject video {
        { QStringLiteral("codec_type"), QStringLiteral("video") },
        { QStringLiteral("codec_name"), QStringLiteral("h264") },
        { QStringLiteral("width"), width },
        { QStringLiteral("height"), height },
        { QStringLiteral("avg_frame_rate"), plan.value(QStringLiteral("sourceFps"))
              .toString(QStringLiteral("%1/1").arg(fps, 0, 'f', 0)) },
        { QStringLiteral("r_frame_rate"), plan.value(QStringLiteral("sourceFps"))
              .toString(QStringLiteral("%1/1").arg(fps, 0, 'f', 0)) },
        { QStringLiteral("nb_read_frames"), sourceFrames },
        { QStringLiteral("duration"), plan.value(QStringLiteral("sourceDuration"))
              .toString(QString::number(defaultDuration, 'f', 6)) }
    };
    if (plan.contains(QStringLiteral("rotation"))) {
        video.insert(QStringLiteral("side_data_list"), QJsonArray {
            QJsonObject {
                { QStringLiteral("side_data_type"),
                  QStringLiteral("Display Matrix") },
                { QStringLiteral("rotation"),
                  plan.value(QStringLiteral("rotation")).toInt() }
            }
        });
    }
    QJsonArray streams;
    if (!plan.value(QStringLiteral("missingVideoStream")).toBool()) {
        streams.append(video);
    }
    const QJsonArray audio = plan.value(QStringLiteral("audioCodecs")).toArray();
    for (const QJsonValue& codec : audio) {
        streams.append(QJsonObject {
            { QStringLiteral("codec_type"), QStringLiteral("audio") },
            { QStringLiteral("codec_name"), codec.toString() }
        });
    }
    return QJsonObject {
        { QStringLiteral("streams"), streams },
        { QStringLiteral("format"), QJsonObject {
            { QStringLiteral("duration"), plan.value(QStringLiteral("containerDuration"))
                  .toString(plan.value(QStringLiteral("sourceDuration"))
                  .toString(QString::number(defaultDuration, 'f', 6))) }
        } }
    };
}

QJsonObject encodedContract(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        return {};
    }
    return QJsonDocument::fromJson(file.readAll()).object();
}

QJsonObject finalProbe(const QJsonObject& plan, const QString& path)
{
    const QJsonObject contract = encodedContract(path);
    const int frames = contract.value(QStringLiteral("frames")).toInt();
    const double fps = contract.value(QStringLiteral("fps")).toDouble();
    QJsonObject video {
        { QStringLiteral("codec_type"), QStringLiteral("video") },
        { QStringLiteral("codec_name"), plan.value(QStringLiteral("finalCodec")).toString(contract.value(QStringLiteral("codec")).toString(QStringLiteral("h264"))) },
        { QStringLiteral("pix_fmt"), plan.value(QStringLiteral("finalPixelFormat")).toString(contract.value(QStringLiteral("pixelFormat")).toString(QStringLiteral("yuv420p"))) },
        { QStringLiteral("width"), plan.value(QStringLiteral("finalWidth")).toInt(contract.value(QStringLiteral("width")).toInt()) },
        { QStringLiteral("height"), plan.value(QStringLiteral("finalHeight")).toInt(contract.value(QStringLiteral("height")).toInt()) },
        { QStringLiteral("nb_frames"), QString::number(
              plan.value(QStringLiteral("finalFrames")).toInt(frames)) },
        { QStringLiteral("avg_frame_rate"), plan.value(QStringLiteral("finalFps")).toString(
              QStringLiteral("%1/1000000").arg(qRound64(fps * 1000000.0))) },
        { QStringLiteral("duration"), plan.value(QStringLiteral("finalDuration"))
              .toString(QString::number(frames / fps, 'f', 9)) }
    };
    QJsonArray streams { video };
    QJsonArray codecs = contract.value(QStringLiteral("audioCodecs")).toArray();
    if (plan.contains(QStringLiteral("finalAudioCodecs"))) {
        codecs = plan.value(QStringLiteral("finalAudioCodecs")).toArray();
    }
    for (const QJsonValue& codec : codecs) {
        streams.append(QJsonObject {
            { QStringLiteral("codec_type"), QStringLiteral("audio") },
            { QStringLiteral("codec_name"), codec.toString() }
        });
    }
    if (plan.value(QStringLiteral("unexpectedStream")).toBool()) {
        streams.append(QJsonObject {
            { QStringLiteral("codec_type"), QStringLiteral("subtitle") },
            { QStringLiteral("codec_name"), QStringLiteral("mov_text") }
        });
    }
    return QJsonObject {
        { QStringLiteral("streams"), streams },
        { QStringLiteral("format"), QJsonObject {
            { QStringLiteral("format_name"), plan.value(QStringLiteral("formatName"))
                  .toString(contract.value(QStringLiteral("format")).toString(QStringLiteral("mov,mp4,m4a,3gp,3g2,mj2"))) },
            { QStringLiteral("duration"), QString::number(frames / fps, 'f', 9) }
        } }
    };
}

int runFfprobe(const QStringList& arguments, const QJsonObject& plan)
{
    const QString input = arguments.last();
    const bool final = input.contains(QStringLiteral(".part."));
    if ((plan.value(QStringLiteral("probeFailure")).toBool() && !final)
        || (plan.value(QStringLiteral("finalProbeFailure")).toBool() && final)) {
        QTextStream(stderr) << "synthetic probe failure\n";
        return 7;
    }
    if ((plan.value(QStringLiteral("malformedProbe")).toBool() && !final)
        || (plan.value(QStringLiteral("malformedFinalProbe")).toBool() && final)) {
        QTextStream(stdout) << "{broken";
        return 0;
    }
    if (arguments.contains(QStringLiteral("-show_frames"))) {
        QTextStream stream(stdout);
        if (plan.value(QStringLiteral("invalidTimeline")).toBool()) {
            stream << "best_effort_timestamp_time=N/A|duration_time=N/A\n";
            return 0;
        }
        const int frames = plan.value(QStringLiteral("frames")).toInt(4);
        const double fps = plan.value(QStringLiteral("fps")).toDouble(4.0);
        const QJsonArray timestamps = plan.value(QStringLiteral("timestamps")).toArray();
        for (int index = 0; index < frames; ++index) {
            const double timestamp = timestamps.isEmpty() ? index / fps : timestamps[index].toDouble();
            stream << "best_effort_timestamp_time=" << QString::number(timestamp, 'f', 9);
            if (!plan.value(QStringLiteral("omitFrameDuration")).toBool())
                stream << "|duration_time=" << QString::number(1.0 / fps, 'f', 9);
            stream << '\n';
            stream.flush();
        }
        return 0;
    }
    const QJsonObject result = final
        ? finalProbe(plan, input) : sourceProbe(plan);
    QTextStream(stdout) << QJsonDocument(result).toJson(QJsonDocument::Compact);
    return 0;
}

int runFfmpeg(const QStringList& arguments, const QJsonObject& plan)
{
    const QString output = arguments.last();
    if (output == QStringLiteral("-")) {
        const bool fullDecodeContract =
            valueAfter(arguments, QStringLiteral("-v")) == QStringLiteral("error")
            && arguments.contains(QStringLiteral("-xerror"))
            && valueAfter(arguments, QStringLiteral("-err_detect"))
                == QStringLiteral("explode")
            && arguments.contains(QStringLiteral("0:v:0"))
            && arguments.contains(QStringLiteral("0:a?"))
            && valueAfter(arguments, QStringLiteral("-f")) == QStringLiteral("null");
        if (!fullDecodeContract) {
            QTextStream(stderr) << "synthetic incomplete decode validation\n";
            return 10;
        }
        if (plan.value(QStringLiteral("decodeFailure")).toBool()) {
            QTextStream(stderr) << "synthetic decode corruption\n";
            return 9;
        }
        if (plan.value(QStringLiteral("decodeWarning")).toBool())
            QTextStream(stderr) << "synthetic recoverable decode error\n";
        if (arguments.contains(QStringLiteral("-progress"))) {
            const auto contract = encodedContract(valueAfter(arguments, QStringLiteral("-i")));
            const int frames = plan.value(QStringLiteral("decodedFrames"))
                .toInt(contract.value(QStringLiteral("frames")).toInt());
            QTextStream(stdout) << "frame=" << frames << "\nprogress=end\n";
        }
        return 0;
    }

    if (arguments.contains(QStringLiteral("-xerror"))) {
        const QString input = valueAfter(arguments, QStringLiteral("-i"));
        const bool imageDecodeContract =
            valueAfter(arguments, QStringLiteral("-v")) == QStringLiteral("error")
            && valueAfter(arguments, QStringLiteral("-err_detect")) == QStringLiteral("explode")
            && valueAfter(arguments, QStringLiteral("-map")) == QStringLiteral("0:v:0")
            && valueAfter(arguments, QStringLiteral("-frames:v")) == QStringLiteral("1")
            && valueAfter(arguments, QStringLiteral("-c:v")) == QStringLiteral("png")
            && valueAfter(arguments, QStringLiteral("-f")) == QStringLiteral("image2")
            && input.contains(QStringLiteral(".part."));
        if (!imageDecodeContract) {
            QTextStream(stderr) << "synthetic incomplete image decode validation\n";
            return 10;
        }
        if (plan.value(QStringLiteral("imageDecodeFailure")).toBool()) {
            QTextStream(stderr) << "synthetic image decode corruption\n";
            return 9;
        }
        if (plan.value(QStringLiteral("omitImageDecodeOutput")).toBool()) {
            return 0;
        }
        QSize size = QImage(input).size();
        if (size.isEmpty() && plan.value(QStringLiteral("opaqueImageEncoding")).toBool()) {
            QFile file(input);
            if (!file.open(QIODevice::ReadOnly)) {
                return 4;
            }
            const QJsonObject encoded = QJsonDocument::fromJson(file.readAll()).object();
            size = QSize(encoded.value(QStringLiteral("width")).toInt(),
                         encoded.value(QStringLiteral("height")).toInt());
        }
        if (size.isEmpty()) {
            return 4;
        }
        if (plan.contains(QStringLiteral("imageDecodeWidth"))) {
            size.setWidth(plan.value(QStringLiteral("imageDecodeWidth")).toInt());
        }
        if (plan.value(QStringLiteral("imageDecodeDiagnostics")).toBool()) {
            QTextStream(stderr) << "synthetic recoverable image decode error\n";
        }
        return saveImage(output, size.width(), size.height()) ? 0 : 4;
    }

    const QString requestedCodec = valueAfter(arguments, QStringLiteral("-c:v"));
    const bool gif = requestedCodec == QStringLiteral("gif");
    const bool videoEncode = gif || requestedCodec == QStringLiteral("libx264") || requestedCodec == QStringLiteral("libx265");
    if (videoEncode) {
        const bool encodeContract =
            (gif || (requestedCodec == plan.value(QStringLiteral("expectedCodec")).toString(QStringLiteral("libx264"))
            && valueAfter(arguments, QStringLiteral("-preset")) == plan.value(QStringLiteral("expectedPreset")).toString(QStringLiteral("medium"))
            && valueAfter(arguments, QStringLiteral("-crf")) == plan.value(QStringLiteral("expectedCrf")).toString(QStringLiteral("18"))
            && valueAfter(arguments, QStringLiteral("-pix_fmt")) == plan.value(QStringLiteral("expectedPixelFormat")).toString(QStringLiteral("yuv420p"))))
            && valueAfter(arguments, QStringLiteral("-map_chapters")) == QStringLiteral("-1")
            && valueAfter(arguments, QStringLiteral("-fps_mode")) == QStringLiteral("cfr")
            && valueAfter(arguments, QStringLiteral("-f")) == (gif ? QStringLiteral("gif") : QStringLiteral("mp4"))
            && (gif ? valueAfter(arguments, QStringLiteral("-filter_complex")).contains(QStringLiteral("paletteuse"))
                    : arguments.contains(QStringLiteral("+faststart+use_metadata_tags")));
        if (!encodeContract) {
            QTextStream(stderr) << "synthetic incomplete video encode contract\n";
            return 12;
        }
        const bool audioCopy = valueAfter(arguments, QStringLiteral("-c:a"))
            == QStringLiteral("copy");
        if (audioCopy && plan.value(QStringLiteral("rejectAudioCopy")).toBool()) {
            QTextStream(stderr) << "[mp4 @ fake] Could not find tag for codec aac in stream #1\n";
            return 1;
        }
        if (plan.value(QStringLiteral("encodeFailure")).toBool()) {
            QTextStream(stderr) << "synthetic unrelated encoder failure\n";
            return 8;
        }
        const QString firstInput = valueAfter(arguments, QStringLiteral("-i"));
        const QSize size = imageSize(QFileInfo(firstInput).absolutePath());
        const QString rate = valueAfter(arguments, QStringLiteral("-framerate"));
        const double fps = rate.contains('/')
            ? rate.section('/', 0, 0).toDouble() / rate.section('/', 1, 1).toDouble()
            : rate.toDouble();
        QJsonObject contract {
            { QStringLiteral("frames"), integerAfter(arguments, QStringLiteral("-frames:v"), 1) },
            { QStringLiteral("fps"), fps },
            { QStringLiteral("width"), size.width() },
            { QStringLiteral("height"), size.height() },
            { QStringLiteral("codec"), gif ? QStringLiteral("gif") : requestedCodec == QStringLiteral("libx265") ? QStringLiteral("hevc") : QStringLiteral("h264") },
            { QStringLiteral("pixelFormat"), valueAfter(arguments, QStringLiteral("-pix_fmt")) },
            { QStringLiteral("format"), gif ? QStringLiteral("gif") : QStringLiteral("mp4") }
        };
        QJsonArray codecs;
        const QJsonArray sourceAudio = plan.value(QStringLiteral("audioCodecs")).toArray();
        const bool mapsAudio = arguments.contains(QStringLiteral("1:a"));
        const QString requestedAudioCodec = valueAfter(arguments, QStringLiteral("-c:a"));
        if (mapsAudio && (requestedAudioCodec == QStringLiteral("copy")
                          || requestedAudioCodec == QStringLiteral("aac"))) {
            for (const QJsonValue& codec : sourceAudio) {
                codecs.append(audioCopy ? codec : QJsonValue(QStringLiteral("aac")));
            }
        }
        contract.insert(QStringLiteral("audioCodecs"), codecs);
        QDir().mkpath(QFileInfo(output).absolutePath());
        QFile file(output);
        if (!file.open(QIODevice::WriteOnly)) {
            return 4;
        }
        file.write(QJsonDocument(contract).toJson(QJsonDocument::Compact));
        if (plan.value(QStringLiteral("blockAfterEncodeOutput")).toBool()) {
            file.flush();
            QTextStream(stdout) << "frame=1\n";
            QTextStream(stdout).flush();
            QThread::sleep(30);
        }
        return 0;
    }

    if (output.contains(QLatin1Char('%'))) {
        if (arguments.contains(QStringLiteral("-noautorotate"))) {
            QTextStream(stderr) << "synthetic extraction must preserve display orientation\n";
            return 13;
        }
        if (plan.value(QStringLiteral("omitFrames")).toBool()) {
            return 0;
        }
        const QString input = valueAfter(arguments, QStringLiteral("-i"));
        QSize size = imageSize(input);
        if (plan.contains(QStringLiteral("rotation"))) {
            const int rotation = ((plan.value(QStringLiteral("rotation")).toInt()
                                   % 360) + 360) % 360;
            if (rotation == 90 || rotation == 270) {
                size.transpose();
            }
        }
        return writeFrames(output, integerAfter(arguments, QStringLiteral("-frames:v"), 1), size)
            ? 0 : 4;
    }

    const QString input = valueAfter(arguments, QStringLiteral("-i"));
    const QImage image(input);
    if (plan.value(QStringLiteral("emptyImageEncodeOutput")).toBool()) {
        QFile file(output);
        return file.open(QIODevice::WriteOnly) ? 0 : 4;
    }
    if (!image.isNull() && plan.value(QStringLiteral("opaqueImageEncoding")).toBool()) {
        // The fake encoded format is deliberately unreadable by Qt, like WebP
        // when its optional image plugin is absent. Only our fake FFmpeg decodes it.
        QFile file(output);
        if (!file.open(QIODevice::WriteOnly)) {
            return 4;
        }
        const QJsonObject encoded {
            { QStringLiteral("width"), image.width() },
            { QStringLiteral("height"), image.height() }
        };
        file.write(QJsonDocument(encoded).toJson(QJsonDocument::Compact));
        return 0;
    }
    return !image.isNull() && image.save(output) ? 0 : 4;
}

int runUpscaler(const QString& tool, const QStringList& arguments, const QJsonObject& plan)
{
    if (plan.value(QStringLiteral("engineFailure")).toBool()) {
        QTextStream(stderr) << "synthetic Vulkan failure\n";
        return 11;
    }
    if (plan.value(QStringLiteral("omitEngineOutput")).toBool()) {
        return 0;
    }
    if (plan.value(QStringLiteral("blockEngine")).toBool()) {
        QTextStream(stdout) << "frame=1\n";
        QTextStream(stdout).flush();
        QThread::sleep(30);
        return 0;
    }

    QString input;
    QString output;
    int scale = integerAfter(arguments, QStringLiteral("-s"), 1);
    int requestedFrames = integerAfter(arguments, QStringLiteral("-n"), 0);
    if (tool == QStringLiteral("mpv")) {
        input = arguments.value(1);
        if (input.startsWith(QStringLiteral("mf://"))) {
            input = input.mid(5);
            input = QFileInfo(input).absolutePath();
        }
        output = valueStartingWith(arguments, QStringLiteral("--o="));
        const QString filter = valueStartingWith(arguments, QStringLiteral("--vf=gpu=w="));
        const auto match = QRegularExpression(QStringLiteral(R"((\d+):h=(\d+))")).match(filter);
        const QSize outputSize(match.captured(1).toInt(), match.captured(2).toInt());
        const int count = QFileInfo(input).isDir()
            ? QDir(input).entryList({ QStringLiteral("*.png") }, QDir::Files).size() : 1;
        return output.contains(QLatin1Char('%'))
            ? (writeFrames(output, count, outputSize, plan) ? 0 : 4)
            : (saveImage(output, outputSize.width(), outputSize.height()) ? 0 : 4);
    }

    input = valueAfter(arguments, QStringLiteral("-i"));
    output = valueAfter(arguments, QStringLiteral("-o"));
    const QSize sourceSize = imageSize(input);
    if (tool.startsWith(QStringLiteral("rife"))) {
        return writeFrames(output, requestedFrames, sourceSize, plan) ? 0 : 4;
    }
    const QSize outputSize(sourceSize.width() * scale, sourceSize.height() * scale);
    if (QFileInfo(input).isDir()) {
        const int count = QDir(input).entryList({ QStringLiteral("*.png") }, QDir::Files).size();
        return writeFrames(output, count, outputSize, plan) ? 0 : 4;
    }
    return saveImage(output, outputSize.width(), outputSize.height()) ? 0 : 4;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication application(argc, argv);
    const QString tool = QFileInfo(QCoreApplication::applicationFilePath())
        .completeBaseName().toLower();
    const QDir binaryDirectory(QCoreApplication::applicationDirPath());
    const QString root = QDir(binaryDirectory.filePath(QStringLiteral("../.."))).absolutePath();
    const QStringList arguments = QCoreApplication::arguments().mid(1);
    const QJsonObject plan = readPlan(root);
    logInvocation(root, tool, arguments);

    if (tool == QStringLiteral("ffprobe")) {
        return runFfprobe(arguments, plan);
    }
    if (tool == QStringLiteral("ffmpeg")) {
        return runFfmpeg(arguments, plan);
    }
    return runUpscaler(tool, arguments, plan);
}
