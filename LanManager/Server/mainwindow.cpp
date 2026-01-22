#include "mainwindow.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QLineEdit>
#include <QDateTime>
#include <QMenu>
#include <QMenuBar>
#include <QStatusBar>
#include <QTabWidget>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_server(new TcpServer(this))
    , m_currentClient(-1)
{
    setupUI();
    createMenuBar();
    
    // 连接服务器信号
    connect(m_server, &TcpServer::clientConnected, this, &MainWindow::onClientConnected);
    connect(m_server, &TcpServer::clientDisconnected, this, &MainWindow::onClientDisconnected);
    connect(m_server, &TcpServer::clientInfoUpdated, this, &MainWindow::onClientInfoUpdated);
    connect(m_server, &TcpServer::sysInfoReceived, this, &MainWindow::onSysInfoReceived);
    connect(m_server, &TcpServer::softwareListReceived, this, &MainWindow::onSoftwareListReceived);
    connect(m_server, &TcpServer::installResult, this, &MainWindow::onInstallResult);
    connect(m_server, &TcpServer::uninstallResult, this, &MainWindow::onUninstallResult);
    connect(m_server, &TcpServer::fileTransferProgress, this, &MainWindow::onFileTransferProgress);
    connect(m_server, &TcpServer::logMessage, this, &MainWindow::onLogMessage);
    
    setWindowTitle("局域网远程管理系统 - 服务端");
    resize(1200, 800);
    
    addLog("程序已启动,请点击\"启动服务器\"开始监听");
}

MainWindow::~MainWindow()
{
    m_server->stop();
}

void MainWindow::setupUI()
{
    QWidget* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);
    
    QVBoxLayout* mainLayout = new QVBoxLayout(centralWidget);
    
    // 顶部工具栏
    QHBoxLayout* toolbarLayout = new QHBoxLayout();
    
    m_btnStart = new QPushButton("启动服务器");
    m_btnStop = new QPushButton("停止服务器");
    m_btnStop->setEnabled(false);
    m_btnSelectAll = new QPushButton("全选");
    m_btnDeselectAll = new QPushButton("取消全选");
    
    toolbarLayout->addWidget(m_btnStart);
    toolbarLayout->addWidget(m_btnStop);
    toolbarLayout->addWidget(m_btnSelectAll);
    toolbarLayout->addWidget(m_btnDeselectAll);
    toolbarLayout->addStretch();
    
    m_progressBar = new QProgressBar();
    m_progressBar->setMaximumWidth(200);
    m_progressBar->setVisible(false);
    toolbarLayout->addWidget(m_progressBar);
    
    connect(m_btnStart, &QPushButton::clicked, this, &MainWindow::onStartServer);
    connect(m_btnStop, &QPushButton::clicked, this, &MainWindow::onStopServer);
    connect(m_btnSelectAll, &QPushButton::clicked, this, &MainWindow::onSelectAll);
    connect(m_btnDeselectAll, &QPushButton::clicked, this, &MainWindow::onDeselectAll);
    
    mainLayout->addLayout(toolbarLayout);
    
    // 主分割器
    QSplitter* mainSplitter = new QSplitter(Qt::Horizontal);
    
    // 左侧:客户端列表
    QGroupBox* clientGroup = new QGroupBox("客户端列表");
    QVBoxLayout* clientLayout = new QVBoxLayout(clientGroup);
    
    m_clientTable = new QTableWidget();
    m_clientTable->setColumnCount(5);
    m_clientTable->setHorizontalHeaderLabels({"选择", "计算机名", "IP地址", "MAC地址", "操作系统"});
    m_clientTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_clientTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_clientTable->horizontalHeader()->setStretchLastSection(true);
    m_clientTable->setColumnWidth(0, 50);
    m_clientTable->setColumnWidth(1, 120);
    m_clientTable->setColumnWidth(2, 120);
    m_clientTable->setColumnWidth(3, 140);
    
    connect(m_clientTable, &QTableWidget::itemSelectionChanged, 
            this, &MainWindow::onClientSelectionChanged);
    
    clientLayout->addWidget(m_clientTable);
    
    // 右侧:信息和操作
    QSplitter* rightSplitter = new QSplitter(Qt::Vertical);
    
    // 信息选项卡
    QTabWidget* infoTabs = new QTabWidget();
    
    // 系统信息页
    QWidget* sysInfoPage = new QWidget();
    QVBoxLayout* sysInfoLayout = new QVBoxLayout(sysInfoPage);
    
    m_btnRefreshSysInfo = new QPushButton("刷新系统信息");
    connect(m_btnRefreshSysInfo, &QPushButton::clicked, this, &MainWindow::onRefreshSysInfo);
    
    m_sysInfoText = new QTextEdit();
    m_sysInfoText->setReadOnly(true);
    m_sysInfoText->setFont(QFont("Consolas", 10));
    
    sysInfoLayout->addWidget(m_btnRefreshSysInfo);
    sysInfoLayout->addWidget(m_sysInfoText);
    
    // 软件管理页
    QWidget* softwarePage = new QWidget();
    QVBoxLayout* softwareLayout = new QVBoxLayout(softwarePage);
    
    QHBoxLayout* softwareBtnLayout = new QHBoxLayout();
    m_btnRefreshSoftware = new QPushButton("刷新软件列表");
    m_btnInstall = new QPushButton("分发安装软件");
    m_btnUninstall = new QPushButton("卸载选中软件");
    
    connect(m_btnRefreshSoftware, &QPushButton::clicked, this, &MainWindow::onRefreshSoftware);
    connect(m_btnInstall, &QPushButton::clicked, this, &MainWindow::onInstallSoftware);
    connect(m_btnUninstall, &QPushButton::clicked, this, &MainWindow::onUninstallSoftware);
    
    softwareBtnLayout->addWidget(m_btnRefreshSoftware);
    softwareBtnLayout->addWidget(m_btnInstall);
    softwareBtnLayout->addWidget(m_btnUninstall);
    softwareBtnLayout->addStretch();
    
    m_softwareTree = new QTreeWidget();
    m_softwareTree->setHeaderLabels({"软件名称", "版本", "发布者", "安装日期"});
    m_softwareTree->setColumnWidth(0, 250);
    m_softwareTree->setColumnWidth(1, 100);
    m_softwareTree->setColumnWidth(2, 150);
    m_softwareTree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    
    softwareLayout->addLayout(softwareBtnLayout);
    softwareLayout->addWidget(m_softwareTree);
    
    infoTabs->addTab(sysInfoPage, "系统信息");
    infoTabs->addTab(softwarePage, "软件管理");
    
    // 日志区域
    QGroupBox* logGroup = new QGroupBox("操作日志");
    QVBoxLayout* logLayout = new QVBoxLayout(logGroup);
    
    m_logText = new QTextEdit();
    m_logText->setReadOnly(true);
    m_logText->setFont(QFont("Consolas", 9));
    m_logText->setMaximumHeight(200);
    
    logLayout->addWidget(m_logText);
    
    rightSplitter->addWidget(infoTabs);
    rightSplitter->addWidget(logGroup);
    rightSplitter->setStretchFactor(0, 3);
    rightSplitter->setStretchFactor(1, 1);
    
    mainSplitter->addWidget(clientGroup);
    mainSplitter->addWidget(rightSplitter);
    mainSplitter->setStretchFactor(0, 1);
    mainSplitter->setStretchFactor(1, 2);
    
    mainLayout->addWidget(mainSplitter);
    
    // 状态栏
    m_statusLabel = new QLabel("就绪");
    statusBar()->addWidget(m_statusLabel);
}

void MainWindow::createMenuBar()
{
    QMenu* fileMenu = menuBar()->addMenu("文件(&F)");
    fileMenu->addAction("退出(&X)", this, &QWidget::close);
    
    QMenu* helpMenu = menuBar()->addMenu("帮助(&H)");
    helpMenu->addAction("关于(&A)", [this]() {
        QMessageBox::about(this, "关于", 
            "局域网远程管理系统 v1.0.0\n\n"
            "功能:\n"
            "- 远程获取电脑软硬件配置\n"
            "- 批量分发安装软件\n"
            "- 远程卸载软件");
    });
}

void MainWindow::updateClientList()
{
    m_clientTable->setRowCount(0);
    
    for (qintptr clientId : m_server->getClientIds()) {
        ClientConnection* client = m_server->getClient(clientId);
        if (!client) continue;
        
        int row = m_clientTable->rowCount();
        m_clientTable->insertRow(row);
        
        // 选择框
        QTableWidgetItem* checkItem = new QTableWidgetItem();
        checkItem->setCheckState(Qt::Unchecked);
        checkItem->setData(Qt::UserRole, (qlonglong)clientId);
        m_clientTable->setItem(row, 0, checkItem);
        
        m_clientTable->setItem(row, 1, new QTableWidgetItem(client->computerName));
        m_clientTable->setItem(row, 2, new QTableWidgetItem(client->ipAddress));
        m_clientTable->setItem(row, 3, new QTableWidgetItem(client->macAddress));
        m_clientTable->setItem(row, 4, new QTableWidgetItem(client->osVersion));
    }
}

void MainWindow::updateSysInfoDisplay(const SystemInfo& info)
{
    QString text;
    text += QString("计算机名: %1\n").arg(info.computerName);
    text += QString("操作系统: %1\n").arg(info.osVersion);
    text += QString("CPU: %1\n").arg(info.cpuInfo);
    text += QString("总内存: %1 MB\n").arg(info.totalMemory);
    text += QString("可用内存: %1 MB\n").arg(info.freeMemory);
    text += QString("磁盘: %1\n").arg(info.diskInfo);
    text += QString("MAC地址: %1\n").arg(info.macAddress);
    text += QString("IP地址: %1\n").arg(info.ipAddress);
    
    m_sysInfoText->setText(text);
}

void MainWindow::updateSoftwareList(const QList<SoftwareInfo>& list)
{
    m_softwareTree->clear();
    
    for (const SoftwareInfo& info : list) {
        QTreeWidgetItem* item = new QTreeWidgetItem();
        item->setText(0, info.name);
        item->setText(1, info.version);
        item->setText(2, info.publisher);
        item->setText(3, info.installDate);
        item->setData(0, Qt::UserRole, info.uninstallCmd);
        m_softwareTree->addTopLevelItem(item);
    }
}

QList<qintptr> MainWindow::getSelectedClients()
{
    QList<qintptr> selected;
    
    for (int row = 0; row < m_clientTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_clientTable->item(row, 0);
        if (item && item->checkState() == Qt::Checked) {
            qintptr clientId = item->data(Qt::UserRole).toLongLong();
            selected.append(clientId);
        }
    }
    
    return selected;
}

void MainWindow::addLog(const QString& message)
{
    QString timestamp = QDateTime::currentDateTime().toString("hh:mm:ss");
    m_logText->append(QString("[%1] %2").arg(timestamp).arg(message));
}

void MainWindow::onStartServer()
{
    bool ok;
    int port = QInputDialog::getInt(this, "启动服务器", "监听端口:", 
                                    DEFAULT_PORT, 1, 65535, 1, &ok);
    if (!ok) return;
    
    if (m_server->start(port)) {
        m_btnStart->setEnabled(false);
        m_btnStop->setEnabled(true);
        m_statusLabel->setText(QString("服务器运行中 (端口: %1)").arg(port));
        addLog(QString("服务器已启动,监听端口 %1").arg(port));
    } else {
        QMessageBox::critical(this, "错误", "服务器启动失败!");
    }
}

void MainWindow::onStopServer()
{
    m_server->stop();
    m_btnStart->setEnabled(true);
    m_btnStop->setEnabled(false);
    m_statusLabel->setText("服务器已停止");
    m_clientTable->setRowCount(0);
    m_sysInfoText->clear();
    m_softwareTree->clear();
    addLog("服务器已停止");
}

void MainWindow::onRefreshSysInfo()
{
    QList<qintptr> clients = getSelectedClients();
    if (clients.isEmpty()) {
        // 如果没有勾选,使用当前选中行
        if (m_currentClient > 0) {
            clients.append(m_currentClient);
        } else {
            QMessageBox::information(this, "提示", "请先选择一个客户端");
            return;
        }
    }
    
    for (qintptr clientId : clients) {
        m_server->requestSysInfo(clientId);
    }
}

void MainWindow::onRefreshSoftware()
{
    QList<qintptr> clients = getSelectedClients();
    if (clients.isEmpty()) {
        if (m_currentClient > 0) {
            clients.append(m_currentClient);
        } else {
            QMessageBox::information(this, "提示", "请先选择一个客户端");
            return;
        }
    }
    
    for (qintptr clientId : clients) {
        m_server->requestSoftwareList(clientId);
    }
}

void MainWindow::onInstallSoftware()
{
    QList<qintptr> clients = getSelectedClients();
    if (clients.isEmpty()) {
        QMessageBox::information(this, "提示", "请先勾选要安装的客户端");
        return;
    }
    
    QString filePath = QFileDialog::getOpenFileName(this, "选择安装包",
        QString(), "安装包 (*.exe *.msi);;所有文件 (*.*)");
    
    if (filePath.isEmpty()) return;
    
    QString args = QInputDialog::getText(this, "安装参数", 
        "静默安装参数(可选):", QLineEdit::Normal, "");
    
    int ret = QMessageBox::question(this, "确认", 
        QString("确定要向 %1 台电脑分发安装:\n%2?")
            .arg(clients.size()).arg(filePath));
    
    if (ret != QMessageBox::Yes) return;
    
    m_progressBar->setValue(0);
    m_progressBar->setVisible(true);
    
    for (qintptr clientId : clients) {
        m_server->installSoftware(clientId, filePath, args);
        addLog(QString("开始向客户端 %1 分发: %2").arg(clientId).arg(filePath));
    }
}

void MainWindow::onUninstallSoftware()
{
    QList<qintptr> clients = getSelectedClients();
    if (clients.isEmpty()) {
        QMessageBox::information(this, "提示", "请先勾选要操作的客户端");
        return;
    }
    
    QList<QTreeWidgetItem*> selectedItems = m_softwareTree->selectedItems();
    if (selectedItems.isEmpty()) {
        QMessageBox::information(this, "提示", "请先选择要卸载的软件");
        return;
    }
    
    QString softwareName = selectedItems.first()->text(0);
    QString uninstallCmd = selectedItems.first()->data(0, Qt::UserRole).toString();
    
    if (uninstallCmd.isEmpty()) {
        QMessageBox::warning(this, "错误", "该软件没有卸载命令");
        return;
    }
    
    int ret = QMessageBox::question(this, "确认",
        QString("确定要在 %1 台电脑上卸载:\n%2?")
            .arg(clients.size()).arg(softwareName));
    
    if (ret != QMessageBox::Yes) return;
    
    for (qintptr clientId : clients) {
        m_server->uninstallSoftware(clientId, softwareName, uninstallCmd);
        addLog(QString("向客户端 %1 发送卸载命令: %2").arg(clientId).arg(softwareName));
    }
}

void MainWindow::onSelectAll()
{
    for (int row = 0; row < m_clientTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_clientTable->item(row, 0);
        if (item) {
            item->setCheckState(Qt::Checked);
        }
    }
}

void MainWindow::onDeselectAll()
{
    for (int row = 0; row < m_clientTable->rowCount(); ++row) {
        QTableWidgetItem* item = m_clientTable->item(row, 0);
        if (item) {
            item->setCheckState(Qt::Unchecked);
        }
    }
}

void MainWindow::onClientConnected(qintptr clientId)
{
    updateClientList();
    addLog(QString("客户端 %1 已连接").arg(clientId));
}

void MainWindow::onClientDisconnected(qintptr clientId)
{
    updateClientList();
    m_softwareLists.remove(clientId);
    if (m_currentClient == clientId) {
        m_currentClient = -1;
        m_sysInfoText->clear();
        m_softwareTree->clear();
    }
    addLog(QString("客户端 %1 已断开").arg(clientId));
}

void MainWindow::onClientInfoUpdated(qintptr clientId)
{
    updateClientList();
}

void MainWindow::onSysInfoReceived(qintptr clientId, const SystemInfo& info)
{
    if (clientId == m_currentClient || getSelectedClients().contains(clientId)) {
        updateSysInfoDisplay(info);
    }
    addLog(QString("收到客户端 %1 (%2) 系统信息").arg(clientId).arg(info.computerName));
}

void MainWindow::onSoftwareListReceived(qintptr clientId, const QList<SoftwareInfo>& list)
{
    m_softwareLists[clientId] = list;
    
    if (clientId == m_currentClient || getSelectedClients().contains(clientId)) {
        updateSoftwareList(list);
    }
    addLog(QString("收到客户端 %1 软件列表 (%2 个软件)").arg(clientId).arg(list.size()));
}

void MainWindow::onInstallResult(qintptr clientId, bool success, const QString& message)
{
    m_progressBar->setVisible(false);
    
    ClientConnection* client = m_server->getClient(clientId);
    QString name = client ? client->computerName : QString::number(clientId);
    
    addLog(QString("客户端 %1 安装%2: %3").arg(name)
        .arg(success ? "成功" : "失败").arg(message));
    
    if (!success) {
        QMessageBox::warning(this, "安装失败", 
            QString("客户端 %1 安装失败:\n%2").arg(name).arg(message));
    }
}

void MainWindow::onUninstallResult(qintptr clientId, bool success, const QString& message)
{
    ClientConnection* client = m_server->getClient(clientId);
    QString name = client ? client->computerName : QString::number(clientId);
    
    addLog(QString("客户端 %1 卸载%2: %3").arg(name)
        .arg(success ? "成功" : "失败").arg(message));
    
    if (success) {
        // 刷新软件列表
        m_server->requestSoftwareList(clientId);
    }
}

void MainWindow::onFileTransferProgress(qintptr clientId, int percent)
{
    m_progressBar->setValue(percent);
}

void MainWindow::onLogMessage(const QString& message)
{
    addLog(message);
}

void MainWindow::onClientSelectionChanged()
{
    QList<QTableWidgetItem*> selected = m_clientTable->selectedItems();
    if (selected.isEmpty()) {
        m_currentClient = -1;
        return;
    }
    
    int row = selected.first()->row();
    QTableWidgetItem* idItem = m_clientTable->item(row, 0);
    if (idItem) {
        m_currentClient = idItem->data(Qt::UserRole).toLongLong();
        
        // 如果有缓存的软件列表,显示它
        if (m_softwareLists.contains(m_currentClient)) {
            updateSoftwareList(m_softwareLists[m_currentClient]);
        } else {
            m_softwareTree->clear();
        }
    }
}
