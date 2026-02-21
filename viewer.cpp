#include "viewerWindow.h"
#include <QFile>
#include <QTextStream>
#include <QVBoxLayout>
#include <QMessageBox>

viewerWindow::viewerWindow(QWidget *parent)
    : QMainWindow(parent)
{
    table = new QTableWidget(this);
    setCentralWidget(table);

    loadCSV("activities.csv"); 
}

void viewerWindow::loadCSV(const QString &filePath)
{
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error",
                             "Could not open CSV file.");
        return;
    }

    QTextStream in(&file);
    int row = 0;

    while (!in.atEnd()) {
        QString line = in.readLine();
        QStringList fields = line.split(",");

        if (table->columnCount() < fields.size())
            table->setColumnCount(fields.size());

        table->insertRow(row);

        for (int col = 0; col < fields.size(); ++col) {
            table->setItem(row, col,
                new QTableWidgetItem(fields[col]));
        }

        row++;
    }

    file.close();
}