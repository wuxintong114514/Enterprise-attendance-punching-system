#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMessageBox>
#include <QSqlTableModel>
#include <QSqlError>       // 添加QSqlError头文件
#include <QSqlRecord>      // 添加QSqlRecord头文件
#include <opencv2/opencv.hpp>
#include <opencv2/face.hpp>
#include <QStandardItemModel> // 添加头文件

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();
    static bool createMySqlConn();

private slots:
    void on_startPushButton_clicked();
    void processCameraFrame();

private:
    Ui::MainWindow *ui;
    QStandardItemModel *tableModel; // 添加成员变量

    // 核心组件
    cv::VideoCapture camera;
    QTimer *frameTimer;
    cv::CascadeClassifier face_cascade;
    cv::Ptr<cv::face::LBPHFaceRecognizer> recognizer;
    QSqlTableModel *model, *model_d;

    // 状态变量
    bool isRecognizing;
    bool hasRecognized;  // 新增状态变量
    cv::Mat currentFrame;
    QString lastRecognizedName;
    std::vector<cv::Mat> training_images;  // 添加训练数据存储
    std::vector<cv::Mat> images; // 添加 images 向量

    // 核心方法
    void initCamera();
    void initDatabase();
    void detectAndRecognizeFaces(cv::Mat& img);  // 移除const修饰符
    void loadTrainingData();
};

#endif // MAINWINDOW_H
