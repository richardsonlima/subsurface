#ifndef DOWNLOADFROMDIVECOMPUTER_H
#define DOWNLOADFROMDIVECOMPUTER_H

#include <QDialog>
#include <QThread>
#include <QHash>

namespace Ui{
	class DownloadFromDiveComputer;
}
struct device_data_t;

class DownloadThread : public QThread{
	Q_OBJECT
public:
    explicit DownloadThread(device_data_t* data);
    virtual void run();
private:
	device_data_t *data;
};

class InterfaceThread : public QThread{
	Q_OBJECT
public:
	InterfaceThread(QObject *parent, device_data_t *data) ;
    virtual void run();

Q_SIGNALS:
	void updateInterface(int value);
private:
	device_data_t *data;
};

class QStringListModel;
class DownloadFromDCWidget : public QDialog{
	Q_OBJECT
public:
    explicit DownloadFromDCWidget(QWidget* parent = 0, Qt::WindowFlags f = 0);

public slots:
	void on_ok_clicked();
	void on_cancel_clicked();

	void on_vendor_currentIndexChanged(const QString& vendor);
private:
	Ui::DownloadFromDiveComputer *ui;
	InterfaceThread *thread;
	bool downloading;

	QStringList vendorList;
	QHash<QString, QStringList> productList;

	QStringListModel *vendorModel;
	QStringListModel *productModel;
	void fill_computer_list();
};

#endif