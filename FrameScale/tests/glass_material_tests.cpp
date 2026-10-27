#include "ui/GlassMaterial.h"
#include <QTemporaryDir>
#include <iostream>

// Material composition and cache checks only. Never open, capture or render
// application windows, and never change the Windows transparency preference.
int main(int argc,char** argv) {
    QApplication app(argc,argv);
    QCoreApplication::setOrganizationName("FrameScaleTests");
    QCoreApplication::setApplicationName("GlassMaterial");
    QTemporaryDir temporary;
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,temporary.path());
    auto check=[](bool ok,const char* message){if(!ok){std::cerr<<message<<'\n';std::exit(1);}};
    QSettings settings;
    settings.setValue("appearance","macos-light");
    settings.setValue("glassTransparency",true);
    auto pixel=[](bool light,bool translucent,const QImage& backdrop=QImage()) {
        QImage image(8,8,QImage::Format_ARGB32_Premultiplied);image.fill(Qt::transparent);
        QPainter painter(&image);
        GlassMaterial::paint(painter,image.rect(),backdrop,light,150,0,translucent);
        painter.end();return image.pixelColor(4,4);
    };
    check(!GlassMaterial::reduced(),"Application transparency setting was ignored");
    check(pixel(true,true).alpha()>100&&pixel(true,true).alpha()<230,"Empty light backdrop lost controlled transparency");
    check(pixel(false,true).alpha()>100&&pixel(false,true).alpha()<230,"Empty dark backdrop lost controlled transparency");
    check(pixel(true,false).alpha()==255,"Opaque surface became translucent");
    QImage blueBackdrop(8,8,QImage::Format_RGB32);
    blueBackdrop.fill(QColor(0,35,100));
    const QColor lightGlass=pixel(true,true,blueBackdrop);
    check(lightGlass.blue()-lightGlass.red()>25,
        "Light material washes out background colour with excessive white tint");

    QWidget window;
    QImage source(16,16,QImage::Format_RGB32);source.fill(QColor(100,150,200));
    check(GlassMaterial::updateBackdrop(&window,source),"Initial material was not created");
    check(!window.property("glassBackdrop").value<QImage>().isNull(),"Material image is missing");
    const auto colour=window.property("glassBackdrop").value<QImage>().pixelColor(0,0);
    check(colour.red()!=colour.blue(),"Glass blur discarded all backdrop colour");
    QImage translucent(24,24,QImage::Format_ARGB32_Premultiplied);
    translucent.fill(QColor(30,100,220,96));
    const auto blurredAlpha=GlassMaterial::blur(translucent,24).pixelColor(12,12);
    check(blurredAlpha.alpha()==96 && blurredAlpha.blue()>blurredAlpha.red()+150,
        "Blur flattens translucent window content or loses premultiplied colour");
    translucent.fill(Qt::transparent);
    check(GlassMaterial::blur(translucent,24).pixelColor(12,12).alpha()==0,
        "Blur adds opaque pixels to an empty window backdrop");
    check(!GlassMaterial::updateBackdrop(&window,source),"Unchanged frame rebuilt the material");
    window.setProperty("glassBackdrop",QVariant());
    check(GlassMaterial::updateBackdrop(&window,source),"Cleared material with the same frame key was not rebuilt");
    settings.setValue("glassTransparency",false);
    check(GlassMaterial::reduced()&&pixel(true,true).alpha()==255,"Disabled transparency did not become opaque");
    check(GlassMaterial::updateBackdrop(&window,source),"Disabled transparency did not clear cached material");
    check(!window.property("glassSourceKey").isValid(),"Disabled transparency left a stale frame key");
    settings.setValue("glassTransparency",true);
    check(GlassMaterial::updateBackdrop(&window,source),"Re-enabling transparency did not restore the material");
    settings.setValue("appearance","studio");
    check(GlassMaterial::reduced()&&pixel(false,true).alpha()==255,"Studio theme did not remain opaque");
    GlassMaterial::updateBackdrop(&window,source);
    settings.setValue("appearance","macos-dark");
    check(GlassMaterial::updateBackdrop(&window,source),"Returning from Studio did not rebuild the same frame");
    check(GlassMaterial::updateBackdrop(&window,{}),"Removing media did not clear cached material");
    check(pixel(false,true).alpha()<255,"Removing media disabled translucency");
    // Different live backgrounds must show through without a cached snapshot.
    auto surface=[&](QColor background) {
        QImage image(20,20,QImage::Format_ARGB32_Premultiplied);image.fill(background);
        QPainter painter(&image);
        GlassMaterial::paintWindowSurface(painter,image.rect(),true);
        painter.end();return image.pixelColor(10,10);
    };
    settings.setValue("appearance","macos-light");
    const auto overRed=surface(Qt::red),overBlue=surface(Qt::blue);
    check(overRed.red()>overBlue.red()+60 && overBlue.blue()>overRed.blue()+60,
        "Window glass hides changes behind it");
    const auto transparent=surface(Qt::transparent);
    check(transparent.alpha()>140 && transparent.alpha()<190,"Window tint lost translucency");
    check(transparent.red()>240,"Light window tint is too dark");
    settings.setValue("glassTransparency",false);
    check(surface(Qt::red)==surface(Qt::blue),"Disabled glass still reveals background");
    std::cout<<"Glass material tests passed\n";
}
