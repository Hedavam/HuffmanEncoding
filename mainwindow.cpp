#include "mainwindow.h"

#include<QtWidgets>
#include <QLabel>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)//, bitCounts(256,0) // way to initialize freq vector member without doing it in the header
{
    /* UI Stuff */

    /* Set starting size, min size for mainwindow */
    setMinimumSize(500,500);

    /* Set up central widget */
    centerWidget = new QWidget(this);
    setCentralWidget(centerWidget);

    /* Place intended layout on central widget */
    mainLayout = new QVBoxLayout(centerWidget);
    header = new QHBoxLayout();
    mainLayout->addLayout(header);

    /* Make our UI items - widgets; encode won't be enabled till we load a file */
    load = new QPushButton("load");
    encode = new QPushButton("encode");
    encode->setDisabled(true);
    decode = new QPushButton("decode");
    freqTable = new QTableWidget();

    /* fill in freq table */
    nRows = 256, nCols = 4;
    freqTable -> setRowCount(nRows);
    freqTable -> setColumnCount(nCols);
    freqTable->setHorizontalHeaderLabels(QStringList() << "Bits" << "Character" << "Frequency" << "Encoding");

    /* Place UI items on layout */
    header->addWidget(load);
    header->addWidget(encode);
    header->addWidget(decode);
    header->setAlignment(Qt::AlignLeft);
    mainLayout->addWidget(freqTable, 1); /* stretch of 1, so the table takes up space */

    /* Connect load button to start file reading */
    connect(load, &QPushButton::clicked, this, &MainWindow::loadClicked);

    /* Connect encode button to actually do encoding */
    connect(encode, &QPushButton::clicked, this, &MainWindow::encodeClicked);

    /* Connect decode button to actually do decoding */
    connect(decode, &QPushButton::clicked, this, &MainWindow::decodeClicked);
}

MainWindow::~MainWindow() {}

void MainWindow::loadClicked() {

    /* byte could represent val 0-255 as its made of 8 (binary) bits - for example: A - 65*/

    /* Get file name from user, create QFile instance using fname, read file as bits, count frequencies */
    QString inName = QFileDialog::getOpenFileName();
    QFile inFile(inName);

    bitCounts = QVector<int>(256, 0);

    /* try to open file; return if action is cancelled, return w/ error message for incorrect file permissions */
    if (inName.isEmpty()) {
        return;
    } else if(!inFile.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, "Cancelled", QString("No Reading Permission for File \"%1\"").arg(inName));
        return;
    }

    bitsFile = inFile.readAll();

    if (bitsFile.isEmpty()) {
        QMessageBox::information(this, "Empty file", QString("File \"%1\" is empty").arg(inName));
        return;
    }

    fileSize = bitsFile.size();

    /* Loop thru whole file (using size as limit), convert each byte to its ASCII int value -> use as index to increment frequencies */
    /* example: for file foo.txt w/ "AABC" we loop thru A,A,B,C; convert A -> 65, increase count at index 65 by 1... etc */
    for (int nbyte = 0; nbyte < fileSize; ++nbyte) {
        ++bitCounts[(unsigned char)bitsFile[nbyte]];
    }

    /* hide encoding column */
    freqTable->setColumnHidden(3, true);

    /* fill in table */
    for(int iRow = 0; iRow < nRows; ++iRow) {
        /* fill in 1st col w/ bits (0-256) */
        QTableWidgetItem *bitItems = new QTableWidgetItem();
        bitItems->setData(Qt::DisplayRole, iRow);
        freqTable->setItem(iRow, 0, bitItems);

        /* fill in 2nd col w/ printable symbols, if available */
        QTableWidgetItem *charItems = new QTableWidgetItem(QChar(iRow));
        freqTable->setItem(iRow, 1, charItems);

        /* fill in 3nd col w/ frequencies */
        QTableWidgetItem *freqItems = new QTableWidgetItem();
        /* Hide 0 frequency rows, re-check to see if on new loads, they should be displayed again cuz new chars were introduced */
        if (bitCounts[iRow] == 0) {
            freqTable->setRowHidden(iRow,true);
        } else {
            freqTable->setRowHidden(iRow,false);
        }
        freqItems->setData(Qt::DisplayRole, bitCounts[iRow]);
        freqTable->setItem(iRow, 2, freqItems);
    }

    freqTable->setSortingEnabled(true);

    /* enable encoding only after loading */
    encode->setEnabled(true);
}

void MainWindow::encodeClicked() {

    /* Initialize toDo by adding pairs (char freq., char) */
    QMultiMap<int, QByteArray> toDo; /* already has priority queue behavior since it's implemented as binary tree */
    int nBits = 256;

    for (int bit = 0; bit < nBits; ++bit) {
        if(bitCounts[bit] > 0) {
            //Need to get the character from just the bit number, but also send QByteArray to insert; Fix w/ Online Help: https://forum.copperspice.com/viewtopic.php?t=1640
            QString intermediary = QChar(bit);
            toDo.insert(bitCounts[bit], intermediary.toLatin1());
        }
    }

    /* Since Huffman's encoding only works if we have at least 2 distinct characters: Check toDO size > 2 before doing encoding, maybe display message to user */
    if(toDo.size() < 2) {
        QMessageBox::information(this, "Error", QString("Load Larger File for Encoding"));
        return;
    }

    /* Maps each sequence of symbols to its two children */
    QMap<QByteArray, QPair<QByteArray, QByteArray> > children;


    /* while there's nodes in toDo list, pop two nodes, combine them into one pair, insert back into toDo list */
    int w0, w1; //weights
    QByteArray a0, a1; //characters

    while(toDo.size() > 1) {
        w0 = toDo.begin().key();
        a0 = toDo.begin().value();

        toDo.erase(toDo.constBegin()); //implictly moves to next itm

        w1 = toDo.begin().key();
        a1 = toDo.begin().value();

        toDo.erase(toDo.constBegin());

        children[a0+a1] = qMakePair(a0, a1);

        toDo.insert(w0+w1, a0+a1);
    }

    /* Compute encodings for each character; well, those that show up at least once */

    /* Root node */
    QByteArray current = toDo.begin().value();

    QString charCode; /* string that grows with encoding until we reach target */
    QByteArray target; /* Single Byte Array contanining char we wanna encode */

    /* for results */
    QVector<QString> charCodes(256, "");
    QString fullEncoding;


    /* Find char code for each char (bit) */
    for(int bit = 0; bit < nBits; ++bit) {
        /* skip 0 freq. characters */
        if(bitCounts[bit] == 0) {
            continue;
        }

        /* reset builder, current, target for next char (bit) - weird conversions to turn char (bit) into QByteArray, refer to line 130 or https://forum.copperspice.com/viewtopic.php?t=1640 */
        charCode = "";
        current = toDo.begin().value();
        QString intermediary = QChar(bit);
        target = intermediary.toLatin1();

        /* build encodings by traversing tree towards target, appending 0 on left moves, 1 on right moves */
        while (current != target) {
            if (children[current].first.contains(target)) {
                charCode.append("0");
                current = children[current].first;
                charCodes[bit] = charCode;
            } else {
                charCode.append("1");
                current = children[current].second;
                charCodes[bit] = charCode;
            }
        }
    }

    /* Create the full encoding */
    for(int i = 0; i < fileSize; ++i) {
        fullEncoding.append(charCodes[(unsigned char)bitsFile[i]]);
    }

    /* for the last sequence of bits (which may not be 8), append 0s to the front to preserve same binary value but satisfy loop/converter */
    int encodingSize = fullEncoding.size();
    int numBytes = ((fullEncoding.size() + 7) / 8); //helpful to save in encoded file for easier decoding

    QByteArray fullEncodingBytes;
    QString EightBitEncoding = "";
    int bitNum;

    /* in chunks of 8 bits, work thru fullEncoding */

    //TODO: Investigate issue where decoding breaks for simpleText.txt using AAABCBAD
    for (int encodingPos = 0; encodingPos < encodingSize; encodingPos += 8) {
        /* substring a length 8 string, starting from encodingPos; on the last iteration, this might not be 8 bits */
        EightBitEncoding = fullEncoding.mid(encodingPos, 8);

        /* possibly pad last chunk on last iteration w/ 00...xxx, where xxx represents the chunk we're trying to convert to byte representation */
        if(EightBitEncoding.size() < 8) {
            EightBitEncoding = EightBitEncoding.rightJustified(8, '0');
        }

        /* convert it to unsigned int, then use it to convert to byte */
        bitNum = EightBitEncoding.toUInt(nullptr, 2);
        fullEncodingBytes.append((unsigned char)(bitNum));
    }

    /* fill in table for encodings */
    freqTable->setColumnHidden(3, false);
    for(int iRow = 0; iRow < nBits; ++iRow) {
        /* fill in 4th col w/ encodings */
        QTableWidgetItem *encodingItems = new QTableWidgetItem(charCodes[iRow]);
        freqTable -> setItem(iRow, 3, encodingItems);
    }

    /* Prompt for output file name, instantiate QFile for output using file name */
    QString outName = QFileDialog::getSaveFileName(this, "Save");
    QFile outFile(outName);

    /* if user cancels file operation, just return*/
    if(outName.isEmpty()) {
        return;
    }

    /* if file creation/opening fails, show error, return */
    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::information(this, "Cancelled", QString("Could not write to File \"%1\"").arg(outName));
        return;
    }

    /* use Data Stream to send serialized outputs into file */
    QDataStream out(&outFile);
    out << charCodes << fullEncodingBytes << numBytes << encodingSize;

    /* only encode once */
    encode->setDisabled(true);
}

void MainWindow::decodeClicked() {

    /* Variables to receive serialized data from an encoded file */
    int numBits;
    int numBytes;
    QString fullEncoding;
    QByteArray fullEncodingBytes;
    QVector<QString> charCodes(256, "");

    /* File operations again */
    QString inName = QFileDialog::getOpenFileName();
    QFile inFile(inName);

    if(inName.isEmpty()) {
        return;
    }

    if (!inFile.open(QIODevice::ReadOnly)) {
        QMessageBox::information(this, "Cancelled", QString("Could not open File \"%1\"").arg(inName));
        return;
    }

    /* Data stream operations again, but in reverse w/ in */
    QDataStream in(&inFile);

    in >> charCodes >> fullEncodingBytes >> numBytes >> numBits;

    /* Loop thru each byte in byte encoding -> turn it into string by calculating binary using QString::number(obj, base 2), then right justifying it (does not change value) */
    QString binary;

    for (int encodingPos = 0; encodingPos < numBytes; ++encodingPos) {
        binary = QString::number((unsigned char)fullEncodingBytes[encodingPos], 2); //remember to treat bytes as unsigned chars
        fullEncoding.append(binary.rightJustified(8, '0')); //padded b/cuz our encodings were fleshed out 8 bit "strings", so we gotta match it
    }


    /* Remove padding from last chunk - if it was used in the encoding */
    if (numBits % 8 != 0) {

        /* get front part */
        QString unPaddedFrontPart = fullEncoding.left(fullEncoding.size() - 8);

        /* get last 8 bits */
        QString paddedBackPart = fullEncoding.right(8);

        /* remove 8 - numBits % 8 leading 0s from last 8 */
        QString unPaddedBackPart = paddedBackPart.sliced(8 - numBits % 8);


        /* concatenate w/ + */
        fullEncoding = unPaddedFrontPart + unPaddedBackPart;
    }

    /* Go thru string, build string one char at a time and each time check if it exists in charCodes */
    /* If so, add it to RESULT string, add 1 to the frequency for that character, identify character (we know which one it is based on what index we found the charCode at) */
    QString decodedData;
    QString huffmanBuilder;
    QVector<int> frequencies(256, 0); //index based on charcode idx

    int fullEncodingSize = fullEncoding.size();
    for (int encodingPos = 0; encodingPos < fullEncodingSize; ++encodingPos) {

        huffmanBuilder.append(fullEncoding[encodingPos]);

        if (charCodes.contains(huffmanBuilder)) {
            decodedData.append((char)(charCodes.indexOf(huffmanBuilder)));
            ++frequencies[charCodes.indexOf(huffmanBuilder)];

            huffmanBuilder = "";
        }

    }

    /* fill in table */
    for(int iRow = 0; iRow < 256; ++iRow) {
        /* fill in 1st col w/ bits (0-256) */
        QTableWidgetItem *bitItems = new QTableWidgetItem();
        bitItems->setData(Qt::DisplayRole, iRow);
        freqTable -> setItem(iRow, 0, bitItems);

        /* fill in 2nd col w/ printable symbols, if available */
        QTableWidgetItem *charItems = new QTableWidgetItem(QChar(iRow));
        freqTable -> setItem(iRow, 1, charItems);

        /* fill in 3nd col w/ frequencies */
        QTableWidgetItem *freqItems = new QTableWidgetItem();
        freqItems->setData(Qt::DisplayRole, frequencies[iRow]);

        /* Hide 0 frequency rows, re-check to see if on new loads, they should be displayed again cuz new chars were introduced */
        if (frequencies[iRow] == 0) {
            freqTable->setRowHidden(iRow,true);
        } else {
            freqTable->setRowHidden(iRow,false);
        }

        freqTable -> setItem(iRow, 2, freqItems);

        /* fill in 4th col w/ encodings */
        QTableWidgetItem *encodings = new QTableWidgetItem(charCodes[iRow]);
        freqTable -> setItem(iRow, 3, encodings);
    }

    freqTable->setSortingEnabled(true);

    /* File operations again */
    QString fName = QFileDialog::getSaveFileName(this, "Save");

    QFile outFile(fName);

    if(fName.isEmpty()) {
        return;
    }

    if (!outFile.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        QMessageBox::information(this, "Cancelled", QString("Could not write to File \"%1\"").arg(fName));
        return;
    }

    /* Send decodedData to new file as user desires */
    QDataStream out(&outFile);
    out << decodedData;
}


/* Test Files */

// 1. Empty file //
// 2. A few repeats of same character //
// 3. Short text file //
// 4. Long text file //
// 4. Image file //


