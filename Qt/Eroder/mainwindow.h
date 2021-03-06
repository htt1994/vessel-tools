#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>

namespace Ui {
    class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

public slots:
	void inputFileSelecter();
    void outputTiffFileSelecter();
    void outputBinFileSelecter();
    void eroder();

private:
    Ui::MainWindow *ui;

public:
	QString inputFileName;
    QString outputTiffFileName;
    QString outputBinFileName;

};

#endif // MAINWINDOW_H
