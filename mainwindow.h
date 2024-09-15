#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <prologixgpib.h>
#include <QDebug>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:

    void on_avgEn_stateChanged(int arg1);

    void on_pushButton_clicked();

private:
    Ui::MainWindow *ui;

    PrologixGPIB *gpib = nullptr;
};
#endif // MAINWINDOW_H
