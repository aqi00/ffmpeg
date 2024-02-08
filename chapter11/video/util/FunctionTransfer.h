#ifndef FUNCTIONTRANSFER_H
#define FUNCTIONTRANSFER_H

#include  <functional>

#include <QThread>
#include <QObject>

class FunctionTransfer : public QObject
{
    Q_OBJECT
public:
    explicit FunctionTransfer(QObject *parent = 0);
    ~FunctionTransfer();

    static void init(Qt::HANDLE id);
    static bool isMainThread();

public:
    // 指定函数f在主线程中执行
    static void runInMainThread(std::function<void()> f, bool isBlock = false);

private:
    static Qt::HANDLE gMainThreadId;
    // 在全局数据区实例化一个FunctionTransfer的实例，该实例所在的线程就是主线程。
    static FunctionTransfer *main_thread_forward;

Q_SIGNALS:
    // 在别的线程有函数对象传来
    void comming(std::function<void()> f);
    void comming_noBlock(std::function<void()> f);

private Q_SLOTS:
    // 执行函数对象
    void slotExec(std::function<void()> f);

};

#endif // FUNCTIONTRANSFER_H
