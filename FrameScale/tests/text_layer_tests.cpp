#include "ui/LayerEditor.h"
#include "ui/Typography.h"
#include "ui/TextLayerOverlay.h"
#include <QTemporaryDir>
#include <iostream>

int main(int argc,char** argv) {
    QApplication app(argc,argv);QApplication::setStyle("Fusion");
    QTemporaryDir settings;QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
    QSettings().setValue("appearance",qEnvironmentVariableIsSet("FRAMESCALE_CAPTURE_LIGHT")?"macos-light":"macos-dark");QSettings().setValue("reduceMotion",true);
#ifdef Q_OS_WIN
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
    QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeuib.ttf");
#endif
    Typography::apply();Appearance::palette();
    if(argc>1) {
        TextLayerDialog dialog(nullptr);dialog.setAttribute(Qt::WA_DontShowOnScreen);
        framescale::TextLayerOptions text;text.content="FrameScale\nCamadas de texto";
        text.fontFamily="Segoe UI";text.stroke=true;
        dialog.setTextOptions(text);dialog.show();app.processEvents();
        QDir().mkpath(QString::fromLocal8Bit(argv[1]));
        dialog.grab().save(QString::fromLocal8Bit(argv[1])+"/text-dialog.png");
        auto* fontList=dialog.findChild<QFontComboBox*>("textFont");
        for(int i=0;i<40;++i) fontList->addItem(QString("Font sample %1").arg(i));
        fontList->showPopup();app.processEvents();
        if(fontList->view()->window()->height()>250) {std::cerr<<"Font popup exceeds compact height\n";return 1;}
        fontList->view()->window()->grab().save(QString::fromLocal8Bit(argv[1])+"/font-menu.png");fontList->hidePopup();
        auto* styleList=dialog.findChild<QComboBox*>("textStyle");styleList->showPopup();app.processEvents();
        for(auto* popup:QApplication::topLevelWidgets()) if(popup->objectName()=="selectionPopup" && popup->isVisible()) popup->grab().save(QString::fromLocal8Bit(argv[1])+"/style-menu.png");styleList->hidePopup();
        QTimer::singleShot(0,[&]{for(auto* window:QApplication::topLevelWidgets()) if(auto* picker=qobject_cast<QColorDialog*>(window)) {
            picker->grab().save(QString::fromLocal8Bit(argv[1])+"/color-dialog.png");picker->reject();
        }});
        for(auto* button:dialog.findChildren<QPushButton*>()) if(button->text().startsWith("#")) {button->click();break;}

        LayerEditor tools;tools.setAttribute(Qt::WA_DontShowOnScreen);framescale::ProcessingOptions p;p.layered=true;
        framescale::MediaLayer l;l.kind=framescale::LayerKind::Text;l.sourceDuration=l.out=5;l.width=1280;l.height=720;l.text=text;p.layers={l};
        tools.setProject({},p,true);tools.selectLayer(0);tools.resize(900,400);
        tools.openTextSettings();
        auto* properties=tools.findChild<QDialog*>("textLayerDialog");
        auto* tabs=properties->findChild<QListWidget*>("settingsSections");tabs->setCurrentRow(1);app.processEvents();
        properties->grab().save(QString::fromLocal8Bit(argv[1])+"/text-transform.png");
        auto video=l;video.kind=framescale::LayerKind::Video;
        dynamic_cast<TextLayerDialog*>(properties)->setLayer(video,QSize(1280,720));app.processEvents();
        properties->grab().save(QString::fromLocal8Bit(argv[1])+"/video-transform.png");
        if(properties->height()>340) {std::cerr<<"Transform window retains empty vertical space\n";return 1;}
        auto* reset=properties->findChild<QPushButton*>("resetLayerTransform");
        reset->setAttribute(Qt::WA_UnderMouse,true);reset->update();app.processEvents();
        properties->grab().save(QString::fromLocal8Bit(argv[1])+"/reset-hover.png");

        tools.resize(900,tools.sizeHint().height());tools.show();app.processEvents();tools.grab().save(QString::fromLocal8Bit(argv[1])+"/transform-tools.png");
        framescale::renderTextLayer(text,1280,720).save(QString::fromLocal8Bit(argv[1])+"/text-render.png");
        QLabel stage;stage.setAttribute(Qt::WA_DontShowOnScreen);stage.resize(640,360);
        QPixmap frame(640,360);frame.fill(QColor("#284c69"));stage.setPixmap(frame);
        TextLayerOverlay overlay(&stage);overlay.setGeometry(stage.rect());
        framescale::MediaLayer title;title.kind=framescale::LayerKind::Text;title.sourceDuration=title.out=5;title.text=text;
        std::vector<framescale::MediaLayer> titles{title};
        overlay.setView(stage.rect(),QSize(1280,720),0);overlay.setLayers(titles,0);
        overlay.contentChanged=[&](int,const std::string& value){titles[0].text.content=value;overlay.setLayers(titles,0);};
        stage.show();app.processEvents();
        stage.grab().save(QString::fromLocal8Bit(argv[1])+"/live-text-before.png");
        const QPointF point=overlay.textRect(0).center();
        QMouseEvent click(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&overlay,&click);
        auto* input=overlay.findChild<QPlainTextEdit*>("canvasTextEditor");input->selectAll();input->insertPlainText("Texto em tempo real");
        app.processEvents();stage.grab().save(QString::fromLocal8Bit(argv[1])+"/live-text-editing.png");
        QImage caretCapture=stage.grab().toImage();
        QPainter caretPainter(&caretCapture);
        caretPainter.translate(input->viewport()->mapTo(&stage,QPoint()));
        dynamic_cast<CanvasTextEdit*>(input)->paintCanvas(caretPainter,true);caretPainter.end();
        caretCapture.save(QString::fromLocal8Bit(argv[1])+"/live-text-caret.png");
        return 0;
    }
    int failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
    {
        QWidget host;TransformValue value(&host);value.setRange(-36000,36000);value.setValue(24);value.resize(160,26);host.show();app.processEvents();
        auto* input=value.findChild<QLineEdit*>();
        auto drag=[&](int start) {
            QMouseEvent down(QEvent::MouseButtonPress,QPointF(start,10),QPointF(start,10),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(input,&down);
            QMouseEvent move(QEvent::MouseMove,QPointF(start+20,10),QPointF(start+20,10),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(input,&move);
            QMouseEvent up(QEvent::MouseButtonRelease,QPointF(start+20,10),QPointF(start+20,10),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
            QApplication::sendEvent(input,&up);
        };
        drag(130);check(value.value()==24,"Dragging blank space changes transform value");
        drag(4);check(value.value()==44,"Dragging number no longer changes transform value");
    }
    framescale::ProcessingOptions project;project.layered=true;
    framescale::MediaLayer layer;layer.sourceDuration=layer.out=10;layer.path="fixture.mp4";
    layer.width=1280;layer.height=720;project.layers={layer};
    LayerEditor editor;editor.setProject("fixture.mp4",project,true);editor.addTextLayer();
    auto* tracks=dynamic_cast<TrimTimeline*>(editor.findChild<QWidget*>("layerTracks"));
    tracks->resize(900,260);
    auto clickTrack=[&](const QPointF& point) {
        QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(tracks,&press);QApplication::sendEvent(tracks,&release);
    };
    int requested=-1;editor.textEditRequested=[&](int index){requested=index;};
    clickTrack(QPointF(tracks->pixelAt(2),79));
    check(!editor.findChild<QDialog*>("textLayerDialog"),"Clicking a text layer opened settings instead of requesting inline editing");
    check(requested==0,"Clicking a text layer did not request preview text editing");
    clickTrack(QPointF(tracks->pixelAt(5)-25,79));
    auto* dialog=dynamic_cast<TextLayerDialog*>(editor.findChild<QDialog*>("textLayerDialog"));
    check(dialog && dialog->isVisible() && !dialog->isModal(),"Text settings icon did not open the modeless dialog");
    if(!dialog) return 1;
    app.processEvents();dialog->adjustSize();app.processEvents();
    check(dialog->findChildren<QScrollArea*>().empty(),"Text properties still require scrolling");
    auto* strokeWidth=dialog->findChild<QDoubleSpinBox*>("textStrokeWidth");
    check(strokeWidth && dialog->rect().contains(QRect(strokeWidth->mapTo(dialog,QPoint()),strokeWidth->size())),
        "Last text property is clipped by the dialog");
    for(auto* label:dialog->findChildren<QLabel*>()) if(label->isVisible())
        {if(label->width()<label->sizeHint().width()) std::cerr<<"Clipped label: "<<label->text().toStdString()<<" actual="<<label->width()<<" hint="<<label->sizeHint().width()<<"\n";check(label->width()>=label->sizeHint().width(),"Text property label is clipped");}

    auto* content=dialog->findChild<QPlainTextEdit*>("textContent");
    content->setPlainText("First");
    check(editor.layers()[0].text.content=="First","Text changes did not reach layer");
    editor.addTextLayer();content->setPlainText("Second");
    editor.selectLayer(1);
    check(content->toPlainText()=="First","Open dialog did not follow selected text layer");
    content->setPlainText("First edited");
    check(editor.layers()[0].text.content=="Second","Editing selection modified another text layer");
    dialog->close();check(!dialog->isVisible(),"Closing dialog failed");
    tracks->resize(900,260);
    const QPointF point(tracks->pixelAt(2),125); // Centre of the second text row.
    QMouseEvent press(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
    QMouseEvent release(QEvent::MouseButtonRelease,point,point,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
    QApplication::sendEvent(tracks,&press);QApplication::sendEvent(tracks,&release);
    check(!dialog->isVisible() && requested==1,"Clicking the text clip reopened settings instead of editing the preview");
    clickTrack(QPointF(tracks->pixelAt(5)-25,125));
    check(dialog->isVisible() && content->toPlainText()=="First edited","Clicking the right-side settings icon did not reopen the selected layer settings");
    dialog->close();editor.setPosition(9);
    double seek=-1;editor.seekRequested=[&](double time){seek=time;};
    clickTrack(QPointF(tracks->pixelAt(2),125));
    check(seek==0 && requested==1 && !dialog->isVisible(),"Selecting text outside its interval did not seek for inline editing");
    editor.selectLayer(2);check(!content->isEnabled(),"Video selection left text controls active");
    editor.selectLayer(0);editor.removeSelected();
    check(content->toPlainText()=="First edited","Deleting a layer left a stale editor target");
    editor.setProject("other.mp4",project,true);
    check(!dialog->isVisible() && !content->isEnabled(),"Project switch left an editable stale dialog");
    {
        // Cursor placement must follow the glyph baseline, not the hidden
        // text control's independent paragraph height and viewport margins.
        framescale::MediaLayer title;title.kind=framescale::LayerKind::Text;title.sourceDuration=title.out=5;
        title.text.content="Textasdaasda";title.text.fontFamily="Segoe UI";
        TextLayerOverlay overlay(nullptr);overlay.resize(960,540);
        for(double zoom:{.237,.5,1.25}) for(int alignment:{0,1,2}) {
            title.text.alignment=alignment;title.text.horizontalScale=125;title.text.verticalScale=80;
            overlay.setView(QRectF(17,23,1920*zoom,1080*zoom),QSize(1920,1080),0);
            overlay.setLayers({title},0);check(overlay.beginEditing(0),"Could not start cursor alignment test");
            auto* input=overlay.findChild<QPlainTextEdit*>("canvasTextEditor");input->moveCursor(QTextCursor::End);
            const QRectF bounds=overlay.textRect(0),caret=overlay.canvasCaretRect();
            check(std::abs(caret.left()-bounds.right())<.1,"Cursor did not follow the last glyph at preview zoom");
            check(std::abs(caret.top()-bounds.top())<.1 && std::abs(caret.height()-bounds.height())<.1,
                "Cursor shifted vertically from the rendered text baseline");
            input->moveCursor(QTextCursor::Start);
            check(std::abs(overlay.canvasCaretRect().left()-bounds.left())<.1,"Cursor start did not align with the first glyph");
        }
        title.text.content="WIDE\nhi";title.text.lineHeight=100;title.text.letterSpacing=3;
        for(int alignment:{0,1,2}) {
            title.text.alignment=alignment;
            overlay.setLayers({title},0);overlay.beginEditing(0);
            auto* input=overlay.findChild<QPlainTextEdit*>("canvasTextEditor");
            auto cursor=input->textCursor();cursor.setPosition(5);input->setTextCursor(cursor);
            const auto bounds=overlay.textRect(0),caret=overlay.canvasCaretRect();
            const QFontMetricsF metrics(framescale::textLayerFont(title.text));
            const double sx=1.25*title.text.horizontalScale/100.,sy=1.25*title.text.verticalScale/100.;
            const double offset=(metrics.horizontalAdvance("WIDE")-metrics.horizontalAdvance("hi"))*(alignment==0?0:alignment==2?1:.5)*sx;
            check(std::abs(caret.x()-bounds.x()-offset)<.1 && std::abs(caret.y()-bounds.y()-100*sy)<.1,
                "Multiline cursor lost line spacing or alignment");
            const QPoint local=input->viewport()->mapFrom(&overlay,(caret.topLeft()+QPointF(1,10)).toPoint());
            QMouseEvent click(QEvent::MouseButtonPress,local,local,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
            QApplication::sendEvent(input->viewport(),&click);
            check(input->textCursor().position()==5,"Clicking a glyph placed the cursor on a different line or character");
        }
    }
    {
        LayerEditor live;live.setProject("fixture.mp4",project,true);live.addTextLayer(false);
        TextLayerOverlay overlay(nullptr);overlay.resize(1280,720);
        overlay.setView(QRectF(0,0,1280,720),QSize(1280,720),0);
        overlay.setLayers(live.layers(),live.selectedLayer());
        live.selectionChanged=[&]{overlay.setLayers(live.layers(),live.selectedLayer());};
        live.textEditRequested=[&](int){overlay.setSelectionTool(false);overlay.finishEditing();};
        overlay.selected=[&](int index){live.selectLayer(index);};
        overlay.contentChanged=[&](int index,const std::string& text) {
            check(index==live.selectedLayer(),"Canvas editor targeted another layer");
            auto value=live.layers()[index].text;value.content=text;live.setSelectedText(value);
        };
        int invalidations=0;live.previewInvalidated=[&]{++invalidations;};
        auto* timeline=dynamic_cast<TrimTimeline*>(live.findChild<QWidget*>("layerTracks"));timeline->resize(900,260);
        const QPointF clipPoint(timeline->pixelAt(2),79);
        QMouseEvent selectPress(QEvent::MouseButtonPress,clipPoint,clipPoint,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QMouseEvent selectRelease(QEvent::MouseButtonRelease,clipPoint,clipPoint,Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(timeline,&selectPress);QApplication::sendEvent(timeline,&selectRelease);
        auto* inlineInput=overlay.findChild<QPlainTextEdit*>("canvasTextEditor");
        check(inlineInput->isHidden() && overlay.cursor().shape()==Qt::IBeamCursor,"Timeline selection stole keyboard focus for text editing");
        const auto count=live.layers().size();
        QKeyEvent remove(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);QApplication::sendEvent(timeline,&remove);
        check(live.layers().size()==count-1 && live.layers()[0].kind==framescale::LayerKind::Video,"Delete did not remove the selected text layer");
        live.undo();check(live.layers().size()==count && live.layers()[0].kind==framescale::LayerKind::Text,"Undo did not restore the deleted text layer");
        check(!live.findChild<QDialog*>("textLayerDialog"),"Timeline text editing opened a settings window");
        const QPointF point=overlay.textRect(0).center();
        QMouseEvent click(QEvent::MouseButtonPress,point,point,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&overlay,&click);
        auto* input=overlay.findChild<QPlainTextEdit*>("canvasTextEditor");
        check(input && input->isVisible(),"Clicking text did not open the inline editor");
        if(input) {
            input->selectAll();
            QKeyEvent key(QEvent::KeyPress,Qt::Key_A,Qt::NoModifier,"A");
            QApplication::sendEvent(input,&key);
            check(live.layers()[0].text.content=="A","Typing over the image did not update the layer immediately");
            check(invalidations==0,"Typing invalidated media playback and triggered a black loading page");
            check(overlay.textRect(0).adjusted(-1,-1,2,1).contains(overlay.canvasCaretRect()),"Inline caret fell outside the rendered text bounds");
            for(const QChar letter:QStringLiteral("asdaasda")) {
                QKeyEvent next(QEvent::KeyPress,0,Qt::NoModifier,QString(letter));QApplication::sendEvent(input,&next);
                const auto caret=overlay.canvasCaretRect(),bounds=overlay.textRect(0);
                check(std::abs(caret.right()-1-bounds.right())<.1 && std::abs(caret.top()-bounds.top())<.1,
                    "Cursor drifted from the text while typing successive letters");
            }
        }
        overlay.setView(QRectF(100,100,640,360),QSize(1280,720),0);
        check(std::abs(overlay.textRect(0).center().x()-420)<2,"Text editor lost alignment after preview zoom/pan");
        auto* selection=live.findChild<QToolButton*>("selectionToolButton");
        live.selectionToolRequested=[&]{overlay.setSelectionTool(true);};
        selection->click();
        check(selection->isChecked() && input->isHidden() && overlay.cursor().shape()==Qt::ArrowCursor,
            "Selection tool did not leave text editing and restore the arrow cursor");
        const auto selectPoint=overlay.textRect(0).center();
        QMouseEvent select(QEvent::MouseButtonPress,selectPoint,selectPoint,Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&overlay,&select);
        check(live.selectedLayer()==0 && input->isHidden(),"Selection tool started typing instead of selecting the layer");
        const auto originalTransform=framescale::layerTransform(live.layers()[0]);
        overlay.transformChanged=[&](int i,const framescale::LayerTransform& t,bool finished){live.selectLayer(i);live.setSelectedTransform(t,true,finished);};
        QMouseEvent drag(QEvent::MouseMove,selectPoint+QPointF(64,36),selectPoint+QPointF(64,36),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&overlay,&drag);
        check(overlay.mask()==QRegion(overlay.rect()),"Dragging changes the mouse input region with the layer");
        QMouseEvent dragMore(QEvent::MouseMove,selectPoint+QPointF(128,72),selectPoint+QPointF(128,72),Qt::NoButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(&overlay,&dragMore);
        QMouseEvent drop(QEvent::MouseButtonRelease,selectPoint+QPointF(128,72),selectPoint+QPointF(128,72),Qt::LeftButton,Qt::NoButton,Qt::NoModifier);
        QApplication::sendEvent(&overlay,&drop);
        auto movedTransform=framescale::layerTransform(live.layers()[0]);
        check(std::abs(movedTransform.x-originalTransform.x-20)<.001 && std::abs(movedTransform.y-originalTransform.y-20)<.001,"Dragging text did not follow preview coordinates");
        check(invalidations==0,"Dragging text restarted media playback");
        live.undo();check(framescale::layerTransform(live.layers()[0])==originalTransform,"One undo did not restore the complete drag gesture");
        live.redo();check(framescale::layerTransform(live.layers()[0])==movedTransform,"Redo lost the drag transform");
        auto* textTool=live.findChild<QToolButton*>("textToolButton");check(textTool && textTool->size()==selection->size(),"Text tool was not placed beside Selection Tool");
        movedTransform.rotation=45;movedTransform.opacity=35;live.setSelectedTransform(movedTransform);
        overlay.setSelectionTool(false);overlay.beginEditing(0);
        check(input->isVisible(),"Transformed text could not be edited");
        const auto bounds=overlay.textRect(0);check(bounds.contains(overlay.canvasCaretRect().topLeft()),"Rotated caret is outside text geometry");
        overlay.setView(QRectF(100,100,640,360),QSize(1280,720),6);
        check(overlay.isHidden(),"Text remained visible beyond its timeline interval");
        live.selectionChanged={};
    }
    {
        LayerEditor live;live.setProject("fixture.mp4",project,true);
        TextLayerOverlay overlay(nullptr);overlay.resize(900,600);overlay.setView(QRectF(100,50,640,360),QSize(1280,720),0);overlay.setLayers(live.layers(),-1);
        live.selectionChanged=[&]{overlay.setLayers(live.layers(),live.selectedLayer());};
        live.textToolRequested=[&]{overlay.armTextTool();};
        live.textCreated=[&](int i){live.setTextToolActive();overlay.beginEditing(i);};
        overlay.createText=[&](const QPointF& p){live.addTextLayer(false,p,true);};
        overlay.selected=[&](int i){live.selectLayer(i);};
        overlay.contentChanged=[&](int i,const std::string& text){auto t=live.layers()[i].text;t.content=text;live.setSelectedText(t);};
        auto* button=live.findChild<QToolButton*>("textToolButton");const auto initial=live.layers().size();button->click();
        QShortcut* textShortcut=nullptr;for(auto* shortcut:live.findChildren<QShortcut*>()) if(shortcut->property("keyboardCommand")=="textTool") textShortcut=shortcut;
        check(textShortcut && textShortcut->key()==QKeySequence("T"),"Text tool shortcut is not T");
        if(textShortcut) QMetaObject::invokeMethod(textShortcut,"activated",Qt::DirectConnection);
        check(live.layers().size()==initial && button->isChecked(),"Text tool created a layer before clicking the preview");
        auto press=[&](const QPointF& p,Qt::MouseButton b=Qt::LeftButton){QMouseEvent e(QEvent::MouseButtonPress,p,p,b,b,Qt::NoModifier);QApplication::sendEvent(&overlay,&e);};
        press(QPointF(20,20));press(QPointF(260,140),Qt::RightButton);
        check(live.layers().size()==initial,"A click outside the image or a right click created text");
        press(QPointF(260,140));
        check(live.layers().size()==initial+1 && live.layers()[0].text.content.empty(),"Preview click did not create an empty text layer");
        check(std::abs(live.layers()[0].text.x-25)<.01 && std::abs(live.layers()[0].text.y-25)<.01,"Text was not created at the preview click coordinates");
        auto* input=overlay.findChild<QPlainTextEdit*>("canvasTextEditor");check(input->isVisible(),"New text was not ready for writing");
        input->insertPlainText("Hello");check(live.layers()[0].text.content=="Hello","Writing into a newly placed text layer failed");
        input->moveCursor(QTextCursor::Start);QKeyEvent eraseCharacter(QEvent::KeyPress,Qt::Key_Delete,Qt::NoModifier);QApplication::sendEvent(input,&eraseCharacter);
        check(live.layers().size()==initial+1 && live.layers()[0].text.content=="ello","Delete while writing removed the layer instead of a character");
        button->click();check(live.layers().size()==initial+1,"Reactivating Text created another layer");
        press(overlay.textRect(0).center());check(live.layers().size()==initial+1,"Clicking existing text created a duplicate layer");
        press(QPointF(600,300));check(live.layers().size()==initial+2,"Clicking another empty point did not create a second text layer");
        live.openTextSettings();auto* properties=dynamic_cast<TextLayerDialog*>(live.findChild<QDialog*>("textLayerDialog"));
        auto* tabs=properties->findChild<QListWidget*>("settingsSections");
        check(tabs && !tabs->item(0)->isHidden() && !tabs->item(1)->isHidden(),"Text properties did not offer Text and Transform together");
        auto* font=properties->findChild<QFontComboBox*>("textFont");
        check(font && !font->isEditable(),"Font field still captures clicks as text editing");
        app.processEvents();
        const QPointF fontPoint(12,font->height()/2.);
        QMouseEvent fontClick(QEvent::MouseButtonPress,fontPoint,font->mapToGlobal(fontPoint.toPoint()),Qt::LeftButton,Qt::LeftButton,Qt::NoModifier);
        QApplication::sendEvent(font,&fontClick);app.processEvents();
        check(font->view()->isVisible(),"Clicking the font field body does not open its list");font->hidePopup();
        auto* alignRight=properties->findChild<QToolButton*>("textAlign2");alignRight->click();
        check(live.layers()[0].text.alignment==2,"Alignment icon does not change text alignment");
        check(properties->findChild<QWidget*>("dialogTitleBar")->height()>=42,"Window drag header remains too thin");
        app.processEvents();
        for(auto* item:properties->findChild<QWidget*>("settingsNavigation")->findChildren<QPushButton*>()) check(item->width()>=item->fontMetrics().horizontalAdvance(item->text())+24,"Property navigation clips its labels");
        check(!live.findChild<QToolButton*>("transformToggle"),"Transform remained on the timeline toolbar");
        tabs->setCurrentRow(1);auto* rotation=properties->findChild<QDoubleSpinBox*>("layerRotation");rotation->setValue(35);
        check(live.layers()[0].text.rotation==35,"Transform tab did not update text rotation");
        live.selectLayer(int(live.layers().size())-1);
        check(!!tabs->item(0)->isHidden() && tabs->currentRow()==1 && rotation->value()==0,"Video properties did not show only Transform");
        rotation->setValue(42);check(live.layers().back().transform.rotation==42,"Transform tab did not update video rotation");
        auto* reset=properties->findChild<QPushButton*>("resetLayerTransform");
        check(reset!=nullptr,"Transform reset button is missing");
        if(reset) {reset->click();check(live.layers().back().transform.isIdentity(),"Reset did not restore video transform");live.undo();check(live.layers().back().transform.rotation==42,"Reset is not a single undoable edit");}
        live.selectLayer(0);check(!tabs->item(0)->isHidden() && tabs->currentRow()==0,"Returning to text did not restore its Text tab");
        live.selectionChanged={};
    }
    if(!failures) std::cout<<"Text settings icon, inline cursor, real-time editing and project isolation passed\n";
    return failures?1:0;
}
