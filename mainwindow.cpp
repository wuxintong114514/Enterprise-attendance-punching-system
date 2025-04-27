#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QDebug>
#include <QFile>
#include <QTimer>
#include <QSqlError>    // 包含QSqlError
#include <QSqlRecord>   // 包含QSqlRecord
#include <QDateTime>    // 添加头文件

// 数据增强函数
cv::Mat augmentImage(const cv::Mat& img) {
    cv::Mat augmentedImg;
    cv::Point2f center(img.cols / 2.0, img.rows / 2.0);
    double angle = (rand() % 30) - 15;
    cv::Mat rot = cv::getRotationMatrix2D(center, angle, 1.0);
    cv::warpAffine(img, augmentedImg, rot, img.size());
    return augmentedImg;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), isRecognizing(false), hasRecognized(false)  // 初始化 hasRecognized
{
    ui->setupUi(this);
    ui->startPushButton->setText("开始识别");

    // 初始化表格模型
    tableModel = new QStandardItemModel(0, 2, this);
    tableModel->setHorizontalHeaderLabels({"姓名", "打卡时间"});
    ui->basicTableView->setModel(tableModel);

    // 初始化数据库
    initDatabase();

    // 初始化摄像头
    initCamera();

    // 加载训练数据
    loadTrainingData();
}

void MainWindow::initCamera()
{
    camera.open(0 + cv::CAP_DSHOW);
    if (!camera.isOpened()) {
        QMessageBox::critical(this, "错误", "无法打开摄像头");
        ui->startPushButton->setEnabled(false);
        return;
    }

    // 配置分类器路径（需确保文件存在）
    QString cascadePath = "haarcascade_frontalface_alt.xml";
    if (!face_cascade.load(cascadePath.toStdString())) {
        QMessageBox::critical(this, "错误",
            QString("人脸检测模型加载失败！\n路径：%1").arg(cascadePath));
        ui->startPushButton->setEnabled(false);
        return;
    }

    // 设置帧处理定时器
    frameTimer = new QTimer(this);
    connect(frameTimer, &QTimer::timeout, this, &MainWindow::processCameraFrame);
    frameTimer->start(33); // ~30 FPS
}

void MainWindow::initDatabase() {
    model_d = new QSqlTableModel(this);
    model_d->setTable("user_profile");
    if (!model_d->select()) {
        qDebug() << "视图加载失败：" << model_d->lastError().text();
    } else {
        qDebug() << "视图记录数：" << model_d->rowCount();
        if (model_d->rowCount() > 0) {
            QSqlRecord firstRecord = model_d->record(0);
            qDebug() << "首条记录字段：" << firstRecord.value("Name").toString()
                     << "| 图片大小：" << firstRecord.value("Picture").toByteArray().size();
        }
    }
}

void MainWindow::processCameraFrame()
{
    cv::Mat frame;
    if (!camera.read(frame)) return;

    cv::cvtColor(frame, currentFrame, cv::COLOR_BGR2RGB);

    if(isRecognizing) {
        detectAndRecognizeFaces(currentFrame);
    }

    // 显示画面
    QImage img(currentFrame.data, currentFrame.cols, currentFrame.rows,
             currentFrame.step, QImage::Format_RGB888);
    ui->CT_Img_Label->setPixmap(QPixmap::fromImage(img.scaled(
        ui->CT_Img_Label->size(), Qt::KeepAspectRatioByExpanding)));
}

void MainWindow::detectAndRecognizeFaces(cv::Mat& img)  // 与头文件声明一致
{
    cv::Mat gray;
    std::vector<cv::Rect> faces;

    cv::cvtColor(img, gray, cv::COLOR_RGB2GRAY);
    cv::equalizeHist(gray, gray);

    face_cascade.detectMultiScale(gray, faces, 1.1, 4);

    for (const auto& face : faces) {
        cv::rectangle(img, face, cv::Scalar(255,0,0), 2);

        // 人脸识别逻辑
        cv::Mat faceROI = gray(face);
        cv::resize(faceROI, faceROI, cv::Size(200,200));

        int label = -1;
        double confidence = 0;
        recognizer->predict(faceROI, label, confidence);

        if (confidence < 120 && label >= 0 && label < model_d->rowCount() && !hasRecognized) {
            QString name = model_d->record(label).value("name").toString();
            cv::putText(img, name.toStdString(), cv::Point(face.x, face.y-10),
                        cv::FONT_HERSHEY_SIMPLEX, 0.9, cv::Scalar(0,255,0), 2);

            QMessageBox::information(this, "识别成功", name+"打卡成功");
            lastRecognizedName = name;
            hasRecognized = true;  // 标记为已识别
            isRecognizing = false;  // 停止识别
            ui->startPushButton->setText("开始识别");

            // 获取当前时间
            QDateTime currentDateTime = QDateTime::currentDateTime();
            QString currentTime = currentDateTime.toString("yyyy-MM-dd HH:mm:ss");

            // 将数据添加到表格模型中
            QList<QStandardItem*> items;
            items.append(new QStandardItem(name));
            items.append(new QStandardItem(currentTime));
            tableModel->appendRow(items);

            break;
        }
    }
}

void MainWindow::on_startPushButton_clicked()
{
    // 前置检查
    if (!camera.isOpened()) {
        QMessageBox::critical(this, "错误", "摄像头未启动");
        return;
    }
    if (face_cascade.empty()) {
        QMessageBox::critical(this, "错误", "人脸检测器未加载");
        return;
    }
    if (training_images.empty()) {
        QMessageBox::critical(this, "错误", "无有效训练数据");
        return;
    }

    // 切换状态
    isRecognizing = !isRecognizing;
    ui->startPushButton->setText(isRecognizing ? "停止识别" : "开始识别");

    if(isRecognizing) {
        hasRecognized = false;  // 重置识别状态
        QMessageBox::information(this, "状态", "识别模式已启动");
    } else {
        lastRecognizedName.clear();
        QMessageBox::information(this, "状态", "识别模式已停止");
    }
}

bool MainWindow::createMySqlConn()
{
    QSqlDatabase db = QSqlDatabase::addDatabase("QMYSQL");
    db.setHostName("localhost");
    db.setDatabaseName("patient");
    db.setUserName("root");
    db.setPassword("114514");

    if (!db.open()) {
        QMessageBox::critical(nullptr, "错误",
            "数据库连接失败: " + db.lastError().text());
        return false;
    }
    return true;
}

void MainWindow::loadTrainingData() {
    recognizer = cv::face::LBPHFaceRecognizer::create();
    std::vector<cv::Mat> images;
    std::vector<int> labels;

    // 检查视图是否有数据
    if (model_d->rowCount() == 0) {
        QMessageBox::critical(this, "错误", "视图中没有数据");
        return;
    }

    for (int i = 0; i < model_d->rowCount(); ++i) {
        QSqlRecord record = model_d->record(i);

        // 按视图字段名获取数据
        QString name = record.value("name").toString();
        QByteArray imgData = record.value("picture").toByteArray();

        qDebug() << "处理记录" << i
                 << "| 姓名:" << name
                 << "| 图片大小:" << imgData.size();

        // 跳过无效数据
        if (name.isEmpty() || imgData.isEmpty()) {
            qDebug() << "跳过无效记录:" << i;
            continue;
        }

        try {
            // 解码图像
            std::vector<uchar> data(imgData.begin(), imgData.end());
            cv::Mat img = cv::imdecode(data, cv::IMREAD_GRAYSCALE);

            if (img.empty()) {
                qDebug() << "图像解码失败:" << name;
                continue;
            }

            // 调整尺寸
            cv::resize(img, img, cv::Size(200, 200));
            if (img.size() != cv::Size(200, 200)) {
                qDebug() << "尺寸调整失败:" << name;
                continue;
            }

            // 添加训练数据
            images.push_back(img);
            labels.push_back(i);  // 使用索引作为标签
            images.push_back(augmentImage(img));
            labels.push_back(i);
            training_images.push_back(img); // 添加到 training_images
            training_images.push_back(augmentImage(img)); // 添加到 training_images
            qDebug() << "成功加载:" << name;

        } catch (cv::Exception &e) {
            qDebug() << "图像处理异常:" << e.what();
        }
    }

    if (images.empty()) {
        QMessageBox::critical(this, "错误", "所有图像数据无效");
        return;
    }

    recognizer->train(images, labels);
    qDebug() << "训练完成，样本数:" << images.size();
}

MainWindow::~MainWindow()
{
    if(frameTimer && frameTimer->isActive()) {
        frameTimer->stop();
    }
    camera.release();
    delete ui;
}
