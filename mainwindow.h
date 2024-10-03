#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtCore/qdir.h>

class QVBoxLayout;
class QHBoxLayout;
class QPushButton;
class QTableWidget;
class QFile;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private:
    /* UI Stuff */
    QWidget *centerWidget;
    QVBoxLayout *mainLayout;
    QHBoxLayout *header;
    QPushButton *load;
    QPushButton *encode;
    QPushButton *decode;
    QTableWidget *freqTable;
    int nRows, nCols;
    QVector<int> bitCounts;
    QByteArray bitsFile;
    int fileSize;



public slots:
    void loadClicked();
    void encodeClicked();
    void decodeClicked();
};
#endif // MAINWINDOW_H
