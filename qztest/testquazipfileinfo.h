#pragma once

#include <QObject>

class TestQuaZipFileInfo : public QObject {
    Q_OBJECT
public:
    explicit TestQuaZipFileInfo(QObject *parent = nullptr);
private slots:
    void testFromFile();
    void testFromDir();
    void testFromLink();
    void testFromZipFile_data();
    void testFromZipFile();
};
