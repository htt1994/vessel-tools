#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QFileDialog>
#include <QFile>
#include <QTextStream>

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
	ui->textEdit->setReadOnly(true);
    QString infoFile = QCoreApplication::applicationDirPath() + "/info/compresser_info.txt";
	QFile file(infoFile);
	bool ok = file.open(QIODevice::ReadOnly | QIODevice::Text);
	if (!ok) {
		ui->textEdit->append("The information file is missing:");
		ui->textEdit->append(infoFile);
		return;
	}
	QTextStream in(&file);
	QString line = in.readLine();
	while (!line.isNull()) {
		ui->textEdit->append(line);
		line = in.readLine();
	}
	file.close();
	ui->textEdit->moveCursor(QTextCursor::Start);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::inputFileSelecter()
{
	ui->labelResult->setText("");
	inputFileName = QFileDialog::getOpenFileName(this,
		tr("Input TIFF file"), ".", tr("TIFF Files (*.tif)"));
	ui->labelInputFile->setText(inputFileName);
}

void MainWindow::outputFileSelecter()
{
	ui->labelResult->setText("");
	outputFileName = QFileDialog::getSaveFileName(this,
		tr("Output TIFF file"), ".", tr("TIFF Files (*.tif)"));
	ui->labelOutputFile->setText(outputFileName);
}

void MainWindow::compresser()
{
	int res;
    QString qstr, resultstr;
    char cmdstr[2048];

    qstr = QCoreApplication::applicationDirPath() + "/exec/compress ";
	qstr += inputFileName;
	qstr += " ";
	qstr += outputFileName;
    if (ui->radioButtonPackbits->isChecked()) {
        qstr += " P";
    } else if (ui->radioButtonLZW->isChecked()) {
        qstr += " L";
    } else if (ui->radioButtonZip->isChecked()) {
        qstr += " Z";
    }

	if (qstr.size()>(int)sizeof(cmdstr)-1) {
		printf("Failed to convert qstr->cmdstr since qstr didn't fit\n");
		resultstr = "FAILED: cmdstr not big enough for the command";
		ui->labelResult->setText(resultstr);
		return;
	}
	strcpy(cmdstr, qstr.toAscii().constData());

	ui->labelResult->setText(cmdstr);
	res = system(cmdstr);
	if (res == 0)
		resultstr = "SUCCESS";
	else if (res == 1)
		resultstr = "FAILED: wrong number of arguments";
	else if (res == 2)
		resultstr = "FAILED: Read error on input file";
	else if (res == 3)
		resultstr = "FAILED: Write error on output file";
	else if (res == 4)
		resultstr = "FAILED: out of memory";
	ui->labelResult->setText(resultstr);
}

