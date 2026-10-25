#include "ui/Appearance.h"
#include "ui/LayerTransformControls.h"
#include "ui/Typography.h"
#include "ui/TextLayerDialog.h"
#include <QFontComboBox>
#include <QTemporaryDir>
#include <QDir>
#include <iostream>

int main(int argc,char** argv) {
 QApplication app(argc,argv);app.setStyle("Fusion");
 QTemporaryDir settings;QSettings::setDefaultFormat(QSettings::IniFormat);QSettings::setPath(QSettings::IniFormat,QSettings::UserScope,settings.path());
 QSettings().setValue("appearance","macos-light");QSettings().setValue("reduceMotion",true);
#ifdef Q_OS_WIN
 QFontDatabase::addApplicationFont("C:/Windows/Fonts/segoeui.ttf");
#endif
 Typography::apply();Appearance::palette();
 int failures=0;auto check=[&](bool ok,const char* message){if(!ok){++failures;std::cerr<<message<<'\n';}};
 // Icons are also used in the installer's 19x15 action-button slots.
 for(const QSize slot:{QSize(19,15),QSize(27,17),QSize(16,26)}) {
  QImage pixels(slot,QImage::Format_ARGB32_Premultiplied);pixels.fill(Qt::transparent);
  QPainter p(&pixels);ControlSymbols::icon(ControlSymbols::Symbol::Plus).paint(&p,pixels.rect());p.end();
  QRect ink;for(int y=0;y<pixels.height();++y) for(int x=0;x<pixels.width();++x)
   if(pixels.pixelColor(x,y).alpha()>50) ink=ink.united(QRect(x,y,1,1));
  check(qAbs(QRectF(ink).center().x()-slot.width()/2.)<=.5 && qAbs(QRectF(ink).center().y()-slot.height()/2.)<=.5,
      "Shared icon is not centred in its rectangular slot");
 }
 QSettings().setValue("glassTransparency",false); // Uniform backdrop isolates hover geometry.
 QDialog window;window.setAttribute(Qt::WA_DontShowOnScreen);window.resize(400,180);
 auto* layout=new QVBoxLayout(&window);
 auto* reset=new LayerIconButton(&window);reset->setIcon(ControlSymbols::icon(ControlSymbols::Symbol::Reset));layout->addWidget(reset);
 auto* combo=new QFontComboBox(&window);combo->setCurrentFont(QFont("Segoe UI"));layout->addWidget(combo);
 auto* mp4=new QComboBox(&window);mp4->addItem("MP4");layout->addWidget(mp4);
 auto* button=new QPushButton("Continuar",&window);layout->addWidget(button);
 Appearance::apply(&window);window.show();app.processEvents();
 const QString dir=argc>1?QString::fromLocal8Bit(argv[1]):QString();if(!dir.isEmpty()) QDir().mkpath(dir);
 int i=0;
 for(QWidget* control:{static_cast<QWidget*>(reset),static_cast<QWidget*>(combo),static_cast<QWidget*>(mp4),static_cast<QWidget*>(button)}) {
  const QRect geometry=control->geometry();
  const QImage normal=window.grab().toImage();
  control->setAttribute(Qt::WA_UnderMouse,true);
  QEnterEvent enter(QPointF(8,8),QPointF(8,8),control->mapToGlobal(QPoint(8,8)));QApplication::sendEvent(control,&enter);
  QMouseEvent move(QEvent::MouseMove,QPointF(8,8),control->mapToGlobal(QPoint(8,8)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(control,&move);
  QHoverEvent hover(QEvent::HoverEnter,QPointF(8,8),control->mapToGlobal(QPoint(8,8)),QPointF(-1,-1));QApplication::sendEvent(control,&hover);
  control->update();app.processEvents();const QImage capture=window.grab().toImage();
  if(!dir.isEmpty()) capture.save(dir+QString("/%1-hover.png").arg(i));
  auto sample=[&](const QImage& image,int x,int y) {auto point=control->mapTo(&window,QPoint(x,y));return image.pixelColor(qRound(point.x()*image.devicePixelRatio()),qRound(point.y()*image.devicePixelRatio()));};
  const int inset=control==reset?3:15;
  const QColor left=sample(capture,inset,5),right=sample(capture,control->width()-1-inset,5);
  const QColor beforeLeft=sample(normal,inset,5),beforeRight=sample(normal,control->width()-1-inset,5);
  const int deltaLeft=left.red()-beforeLeft.red(),deltaRight=right.red()-beforeRight.red();
  std::cout<<"control "<<i<<" underMouse="<<control->underMouse()<<" hover delta "<<deltaLeft<<","<<deltaRight<<'\n';
  check(qAbs(deltaLeft)>3,"Hover did not draw a visible highlight");
  check(qAbs(deltaLeft-deltaRight)<=3,
       "Hover has a darker/offset surface on one side");
  check(geometry==control->geometry(),"Hover changed control geometry");
  control->setAttribute(Qt::WA_UnderMouse,false);QEvent leave(QEvent::Leave);QApplication::sendEvent(control,&leave);control->update();app.processEvents();
  ++i;
 }

 // Reproduce the user's actual leading field, including its embedded editor.
 QSettings().setValue("glassTransparency",true);
 TextLayerDialog properties(nullptr);properties.setAttribute(Qt::WA_DontShowOnScreen);
 properties.show();app.processEvents();
 auto* leading=properties.findChild<QDoubleSpinBox*>("textLeading");
 auto* input=leading->findChild<QLineEdit*>();
 QLabel marker(&properties);marker.setStyleSheet("background:#ff0000;border:0;padding:0;margin:0;");marker.setGeometry(30,properties.height()-15,6,6);marker.show();
 const QImage positionImage=properties.grab().toImage();
 check(!properties.property("nativeGlassActive").toBool(),"Property dialog still uses rectangular native blur");
 for(QPoint corner:{QPoint(0,0),QPoint(positionImage.width()-1,0),QPoint(0,positionImage.height()-1),QPoint(positionImage.width()-1,positionImage.height()-1)})
  check(positionImage.pixelColor(corner).alpha()==0,"Property dialog paints outside rounded corners");

 QRect redBounds;for(int y=0;y<positionImage.height();++y)for(int x=0;x<positionImage.width();++x) {
  const auto c=positionImage.pixelColor(x,y);if(c.red()>240 && c.green()<10 && c.blue()<10 && c.alpha()>240) redBounds=redBounds.united(QRect(x,y,1,1));
 }
 const auto dpr=positionImage.devicePixelRatio();const QRect expected(qRound(marker.x()*dpr),qRound(marker.y()*dpr),qRound(marker.width()*dpr),qRound(marker.height()*dpr));
 std::cerr<<"Marker actual="<<redBounds.x()<<","<<redBounds.y()<<" expected="<<expected.x()<<","<<expected.y()<<"\n";
 // Fractional DPR can leave one antialiased edge pixel below the solid-red
 // threshold; it must not move the marker by the window's inset.
 check((redBounds.topLeft()-expected.topLeft()).manhattanLength()<=1 && qAbs(redBounds.width()-expected.width())<=1 && qAbs(redBounds.height()-expected.height())<=1,
       "Rounded window shifts child painting away from control coordinates");marker.hide();
 const QImage normalField=properties.grab().toImage();
 if(!dir.isEmpty()) normalField.save(dir+"/leading-normal.png");
 for(QWidget* field:{static_cast<QWidget*>(leading),static_cast<QWidget*>(input)}) {
  field->setAttribute(Qt::WA_UnderMouse,true);
  QEnterEvent enter(QPointF(12,10),QPointF(12,10),field->mapToGlobal(QPoint(12,10)));QApplication::sendEvent(field,&enter);
  QMouseEvent move(QEvent::MouseMove,QPointF(12,10),field->mapToGlobal(QPoint(12,10)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(field,&move);
  field->update();
 }
 app.processEvents();
 const QImage fieldHover=properties.grab().toImage();
 if(!dir.isEmpty()) fieldHover.save(dir+"/leading-hover.png");
 auto at=[&](const QImage& image,QPoint local) {
  auto point=leading->mapTo(&properties,local);return image.pixelColor(qRound(point.x()*image.devicePixelRatio()),qRound(point.y()*image.devicePixelRatio()));
 };
 for(const QPoint point:{QPoint(2,leading->height()/2),QPoint(leading->width()/2,3),QPoint(leading->width()-3,leading->height()/2),QPoint(leading->width()/2,leading->height()-4)})
 {
  const auto before=at(normalField,point),after=at(fieldHover,point);
  if(!(after.red()<before.red()-3)) std::cerr<<"Leading sample "<<point.x()<<","<<point.y()<<" normal="<<before.red()<<" hover="<<after.red()<<" field="<<leading->width()<<"x"<<leading->height()<<"\n";
  check(after.red()<before.red()-3,"Leading hover does not cover the complete rounded field");
 }
 // Moving onto either arrow must not paint an inset second rectangle.
 for(int y:{5,leading->height()-5}) {
  const QPointF point(leading->width()-12,y);
  QMouseEvent move(QEvent::MouseMove,point,leading->mapToGlobal(point.toPoint()),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(leading,&move);
  QHoverEvent hover(QEvent::HoverMove,point,leading->mapToGlobal(point.toPoint()),QPointF(12,10));QApplication::sendEvent(leading,&hover);
  leading->update();app.processEvents();const QImage arrowHover=properties.grab().toImage();
  const QPoint background(leading->width()-7,y);
  check(qAbs(at(arrowHover,background).red()-at(fieldHover,background).red())<=2,
        "Leading arrow draws a smaller second hover rectangle");
  if(!dir.isEmpty()) arrowHover.save(dir+QString("/leading-arrow-%1.png").arg(y));
 }

 // The actual font chooser in the inset tool window must cover the same full
 // border-box as the noneditable MP4 chooser, including when its editor is hit.
 auto* font=properties.findChild<QFontComboBox*>("textFont");
 font->setCurrentFont(QFont("Segoe UI"));app.processEvents();
 const QImage fontNormal=properties.grab().toImage();
 for(QWidget* part:{static_cast<QWidget*>(font),static_cast<QWidget*>(font->lineEdit())}) {
  if(!part) continue;
  part->setAttribute(Qt::WA_UnderMouse,true);
  QEnterEvent enter(QPointF(12,10),QPointF(12,10),part->mapToGlobal(QPoint(12,10)));QApplication::sendEvent(part,&enter);
  QMouseEvent move(QEvent::MouseMove,QPointF(12,10),part->mapToGlobal(QPoint(12,10)),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(part,&move);
  part->update();
 }
 app.processEvents();const QImage fontHover=properties.grab().toImage();
 for(const QPoint point:{QPoint(2,font->height()/2),QPoint(font->width()/2,3),QPoint(font->width()-3,font->height()/2),QPoint(font->width()/2,font->height()-4)}) {
  const QPoint pos=font->mapTo(&properties,point);const auto ratio=fontHover.devicePixelRatio();
  const QPoint pixel(qRound(pos.x()*ratio),qRound(pos.y()*ratio));
  check(fontHover.pixelColor(pixel).red()<fontNormal.pixelColor(pixel).red()-3,"Font hover is inset instead of filling its field like MP4");
 }
 if(!dir.isEmpty()) {fontNormal.save(dir+"/font-normal.png");fontHover.save(dir+"/font-hover.png");}
 return failures?1:0;
}
