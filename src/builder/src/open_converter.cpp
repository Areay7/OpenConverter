#include <QMainWindow>
#include <QTranslator>
#include <QMessageBox>
#include <QAction>
#include <QEvent>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QIcon>
#include <QString>
#include <QUrl>
#include <QFileInfo>
#include <QByteArray>
#include <QFileDialog>
#include <QToolButton>
#include <QPushButton>
#include <QMenu>
#include <QStatusBar>
#include <QProgressBar>
#include <QLabel>
#include <QLineEdit>
#include <QThread>
#include <QMetaObject>
#include <QApplication>
#include <QMimeData>


#include "../include/open_converter.h"
#include "../include/encode_setting.h"
#include "../../engine/include/converter.h"
#include "../../common/include/info.h"
#include "../../common/include/encode_parameter.h"
#include "../../common/include/process_parameter.h"
#include "../../common/include/process_observer.h"

#include "ui_open_converter.h"

#include <iostream>

OpenConverter::OpenConverter(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::OpenConverter) {
    /* init objects */
    info = new Info;
    encodeParameter = new EncodeParameter;
    encodeSetting = new EncodeSetting(nullptr, encodeParameter);
    encodeSetting->setWindowTitle("Encode Setting");
    processParameter = new ProcessParameter;
    converter = new Converter(processParameter, encodeParameter);
    displayResult = new QMessageBox;
    transcoderGroup = new QActionGroup(this);
    languageGroup = new QActionGroup(this);

    ui->setupUi(this);
    setAcceptDrops(true);
    setWindowTitle("OpenConverter");
    setWindowIcon(QIcon(":/icon/icon.png"));

    ui->progressBar->setValue(0);

    // Register this class as an observer for process updates
    processParameter->addObserver(std::shared_ptr<ProcessObserver>(this));

    transcoderGroup->setExclusive(true);

    QList<QAction*> transcoderActions = ui->menuTranscoder->actions();
    for (QAction* action : transcoderActions) {
        action->setCheckable(true);
        transcoderGroup->addAction(action);
    }

    if (!transcoderActions.isEmpty()) {
        transcoderActions.first()->setChecked(true);
        converter->set_Transcoder(transcoderActions.first()->objectName().toStdString());
    }

    languageGroup->setExclusive(true);
    QList<QAction*> languageActions = ui->menuLanguage->actions();
    for (QAction* action : languageActions) {
        action->setCheckable(true);
        languageGroup->addAction(action);
    }

    m_currLang = "english";
    m_langPath = ":/";
    for (QAction* action : languageActions) {
        if (action->objectName() == m_currLang) {
            action->setChecked(true);
            break;
        }
    }

    connect(ui->toolButton, &QToolButton::clicked, [&]() {
        QString filename = QFileDialog::getOpenFileName();
        ui->lineEdit_inputFile->setText(filename);
    });

    connect(ui->pushButton_apply, SIGNAL(clicked(bool)), this,
            SLOT(apply_Pushed()));

    connect(ui->pushButton_convert, SIGNAL(clicked(bool)), this,
            SLOT(convert_Pushed()));

    connect(ui->pushButton_encodeSetting, SIGNAL(clicked(bool)), this,
            SLOT(encode_Setting_Pushed()));

    connect(ui->menuLanguage, SIGNAL(triggered(QAction *)), this,
            SLOT(slotLanguageChanged(QAction *)));

    connect(ui->menuTranscoder, SIGNAL(triggered(QAction *)), this,
            SLOT(slotTranscoderChanged(QAction *)));
}

void OpenConverter::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls()) {
        event->acceptProposedAction();
    }
}

void OpenConverter::dropEvent(QDropEvent *event) {
    if (event->mimeData()->hasUrls()) {
        const QUrl url = event->mimeData()->urls().first();
        ui->lineEdit_inputFile->setText(url.toLocalFile());
        event->acceptProposedAction();
    }
}

// Called every time, when a menu entry of the transcoder menu is called
void OpenConverter::slotTranscoderChanged(QAction *action) {
    if (0 != action) {
        std::string transcoderName = action->objectName().toStdString();
        bool isValid = false;
#ifdef ENABLE_FFMPEG
        if (transcoderName == "FFMPEG") {
            converter->set_Transcoder(transcoderName);
            isValid = true;
        }
#endif
#ifdef ENABLE_FFTOOL
        if (transcoderName == "FFTOOL") {
            converter->set_Transcoder(transcoderName);
            isValid = true;
        }
#endif
#ifdef ENABLE_BMF
        if (transcoderName == "BMF") {
            converter->set_Transcoder(transcoderName);
            isValid = true;
        }
#endif
        // If the transcoder name is not valid, log an error
        if (isValid) {
            ui->statusBar->showMessage(
                tr("Current Transcoder changed to %1")
                    .arg(QString::fromStdString(transcoderName)));
        } else {
            std::cout << "Error: Undefined transcoder name - "
                      << transcoderName.c_str() << std::endl;
        }
    }
}

// Called every time, when a menu entry of the language menu is called
void OpenConverter::slotLanguageChanged(QAction *action) {
    if (0 != action) {
        // load the language dependant on the action content
        loadLanguage(action->objectName());
        setWindowIcon(action->icon());
    }
}

void switchTranslator(QTranslator &translator, const QString &filename) {
    // remove the old translator
    qApp->removeTranslator(&translator);

    // load the new translator
    //    QString path = QApplication::applicationDirPath();
    //    path.append(":/");
    if (translator.load(QString(":/%1").arg(
            filename))) // Here Path and Filename has to be entered because the
                        // system didn't find the QM Files else
        qApp->installTranslator(&translator);
}

void OpenConverter::loadLanguage(const QString &rLanguage) {
    if (m_currLang != rLanguage) {
        m_currLang = rLanguage;
        //        QLocale locale = QLocale(m_currLang);
        //        QLocale::setDefault(locale);
        //        QString languageName =
        //        QLocale::languageToString(locale.language());
        switchTranslator(m_translator, QString("lang_%1.qm").arg(rLanguage));
        //        switchTranslator(m_translatorQt,
        //        QString("qt_%1.qm").arg(rLanguage));
        ui->statusBar->showMessage(
            tr("Current Language changed to %1").arg(rLanguage));
    }
}

void OpenConverter::changeEvent(QEvent *event) {
    if (event->type() == QEvent::LanguageChange) {
        // save the current input and output folders
        currentInputPath = ui->lineEdit_inputFile->text();
        currentOutputPath = ui->lineEdit_outputFile->text();

        ui->retranslateUi(this);

        // restore the input and output folders
        ui->lineEdit_inputFile->setText(currentInputPath);
        ui->lineEdit_outputFile->setText(currentOutputPath);

        if (info && info->get_Quick_Info()) {
            info_Display(info->get_Quick_Info());  // Convert QuickInfo to string
        }
    }
    QMainWindow::changeEvent(event);
}

void OpenConverter::handle_Converter_Result(bool flag) {
    if (flag) {
        displayResult->setText("Convert success!");
        ui->label_timeRequiredResult->setText(QString("%1s").arg(0));
    } else {
        displayResult->setText("Convert failed! Please ensure the file path "
                               "and encode setting is correct");
    }
    displayResult->show();
}

void OpenConverter::onProcessUpdate(double progress) {
    int process = progress;
    ui->progressBar->setValue(process);
    ui->label_processResult->setText(QString("%1%").arg(process));
}

void OpenConverter::onTimeUpdate(double timeRequired) {
    ui->label_timeRequiredResult->setText(
        QString("%1s").arg(QString::number(timeRequired, 'f', 2)));
}

void OpenConverter::encode_Setting_Pushed() { encodeSetting->show(); }

void OpenConverter::apply_Pushed() {

    QByteArray ba = ui->lineEdit_inputFile->text().toLocal8Bit();
    char *src = ba.data();
    // get info by Decapsulation
    info->send_Info(src);

    // display info on window
    info_Display(info->get_Quick_Info());
}

void OpenConverter::convert_Pushed() {

    // get the input file path
    QString inputFilePath = ui->lineEdit_inputFile->text();
    // check the input file path
    if (inputFilePath.isEmpty()) {
        displayResult->setText("Please select an input file.");
        displayResult->exec();
        return;
    }
    // get the output file path
    QString outputFilePath = ui->lineEdit_outputFile->text();
    // if the output file path is empty, generate a default output filename
    if (outputFilePath.isEmpty()) {
        QFileInfo fileInfo(inputFilePath);
        outputFilePath = fileInfo.absolutePath() + "/" +
                         fileInfo.completeBaseName() + "-oc-output." +
                         fileInfo.suffix();
        ui->lineEdit_outputFile->setText(outputFilePath);
    }

    // Check if the input file and output file are the same
    if (inputFilePath == outputFilePath) {
        displayResult->setText(
            "The input file can't be the same as the output file!");
        displayResult->exec();
        return;
    }

    // Start conversion in worker thread

    // capture everything you need by value
    auto* thread = QThread::create([=]() {
        bool ok = converter->convert_Format(
            inputFilePath.toStdString(),
            outputFilePath.toStdString()
        );
        // When done, marshal back to the GUI thread:
        QMetaObject::invokeMethod(this, [this, ok]() {
            handle_Converter_Result(ok);
        }, Qt::QueuedConnection);
    });

    // clean up the QThread object once it finishes
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    // fire off
    thread->start();;

}

// automatically select kbps/Mbps
QString OpenConverter::formatBitrate(int64_t bitsPerSec) {
    const double kbps = bitsPerSec / 1000.0;
    if (kbps >= 1000.0) {
        return QString("%1 Mbps").arg(kbps / 1000.0, 0, 'f', 1);
    }
    return QString("%1 kbps").arg(kbps, 0, 'f', 1);
}

// automatically select Hz/kHz/MHz
QString OpenConverter::formatFrequency(int64_t hertz) {
    const double kHz = hertz / 1000.0;
    if (kHz >= 1000.0) {
        return QString("%1 MHz").arg(kHz / 1000.0, 0, 'f', 2);
    } else if (kHz >= 1.0) {
        return QString("%1 kHz").arg(kHz, 0, 'f', 1);
    }
    return QString("%1 Hz").arg(hertz);
}

void OpenConverter::info_Display(QuickInfo *quickInfo) {
    if (!quickInfo) return;

    // video
    ui->label_videoStreamResult->setText(
        QString("%1").arg(quickInfo->videoIdx));
    ui->label_widthResult->setText(
        QString("%1 px").arg(quickInfo->width));
    ui->label_heightResult->setText(
        QString("%1 px").arg(quickInfo->height));
    ui->label_colorSpaceResult->setText(
        QString("%1").arg(QString::fromStdString(quickInfo->colorSpace)));
    ui->label_videoCodecResult->setText(
        QString("%1").arg(QString::fromStdString(quickInfo->videoCodec)));
    ui->label_videoBitRateResult->setText(
        formatBitrate(quickInfo->videoBitRate));
    ui->label_frameRateResult->setText(
        QString("%1 fps").arg(quickInfo->frameRate,0,'f',2));
    // audio
    ui->label_audioStreamResult->setText(
        QString("%1").arg(quickInfo->audioIdx));
    ui->label_audioCodecResult->setText(
        QString("%1").arg(QString::fromStdString(quickInfo->audioCodec)));
    ui->label_audioBitRateResult->setText(
        formatBitrate(quickInfo->audioBitRate));
    ui->label_channelsResult->setText(
        QString("%1").arg(quickInfo->channels));
    ui->label_sampleFmtResult->setText(
        QString("%1").arg(QString::fromStdString(quickInfo->sampleFmt)));
    ui->label_sampleRateResult->setText(
        formatFrequency(quickInfo->sampleRate));
}

OpenConverter::~OpenConverter() {
    delete ui;
    delete info;
    delete encodeParameter;
    delete encodeSetting;
    delete processParameter;
    delete converter;
    delete displayResult;
}

#include "open_converter.moc"
