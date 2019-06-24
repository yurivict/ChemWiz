#include <QApplication>
#include <QWebEngineView>

int main(int argc, char *argv[]) {
  QApplication a(argc, argv);

  QWebEngineView view;
  view.resize(1024, 750);
  view.show();
  view.load(QUrl(argv[1]));

  return a.exec();
}

