#pragma once

#include <QObject>

class TestQuaZipFileInfo : public QObject {
    Q_OBJECT
public:
    explicit TestQuaZipFileInfo(QObject *parent = nullptr);
private slots:
    void testFromFile_data();
    void testFromFile();
    void testFromDir_data();
    void testFromDir();
    void testFromLink_data();
    void testFromLink();
    void testFromZipFile_data();
    void testFromZipFile();
};
