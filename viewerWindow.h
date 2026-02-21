#ifndef VIEWERWINDOW_H
#define VIEWERWINDOW_H

#include <QMainWindow>
#include <QTableWidget>

class viewerWindow : public QMainWindow
{
    Q_OBJECT

public:
    viewerWindow(QWidget *parent = nullptr);

private:
    QTableWidget *table;
    void loadCSV(const QString &filePath);
};

#endif