#pragma once

#include <ui/task/file.fwd.h>
#include <ui/task/task.fwd.h>

#include <QString>
#include <QAbstractTableModel>

#include <set>

namespace ui {
namespace task {


/// Generic container that represent abstract chunk of data that will be stored as file on back-end.
/// Derive new classes from it if you need to create new types of Node while re-use serialization routines
// class FileMixin
// {
// public:

// 	//virtual QString type() const = 0;

// private:
// 	virtual QByteArray data() const = 0;
// 	virtual void data(QByteArray const &) = 0;

// 	//virtual QByteArray file_data() const = 0;
// 	//virtual void file_data(QByteArray const &) = 0;

// 	virtual QString file_name() const = 0;


// 	// return true if underlying object contain no data
// 	virtual bool empty() const = 0;
// };

struct FileID
{
	enum class Kind {none, input, output};

	//explicit FileID(Kind kind=Kind::none, QString name=QString()) : kind_(kind), name_(name) {}
	explicit FileID(Kind kind, QString name) : kind_(kind), name_(name) {}

	Kind kind_ = Kind::none;
	QString name_; // remote file name (shold not be empty)

	bool operator ==(FileID const &r) const { return kind_ == r.kind_  and  name_ == r.name_; }
	bool operator !=(FileID const &r) const { return not (*this == r); }
};


/// File node that hold concreate data with concreate file-name
class File final : private FileID
{
public:
	using FileID::Kind;


	//explicit File();
	explicit File(Kind kind = Kind::none, QString const & name = QString(), QByteArray const & file_data = QByteArray());

	FileID file_id() const { return *this; }

	Kind kind() const { return kind_; }
	void kind(Kind kind) { kind_ = kind; }

	QString const & name() const { return name_; }
	void name(QString const &_name) { name_ = _name; }

	static FileSP init_from_local_file(QString const & local_file_name);

	//QString type() const override { return "file"; };

	QByteArray data() const;
	void data(QByteArray const &_file_data);

    //QByteArray file_data() const override { return file_data_; };
	//void file_data(QByteArray const &_file_data) override { file_data_ = _file_data; }

	QString hash() const { return hash_; }
	void hash(QString const &_hash) { hash_ = _hash; }

	QString local_file_name() const { return local_file_name_; }

	bool empty() const { return file_data_.isEmpty(); }
	//bool null() const override { return file_data_.isNull(); }

	File& operator=(File&& other) noexcept;


	bool operator ==(File const &r) const;
	bool operator !=(File const &r) const { return not (*this == r); }

	// serialization
	friend QDataStream &operator<<(QDataStream &, File const&);
	friend QDataStream &operator>>(QDataStream &, File &);

	friend QDataStream &operator<<(QDataStream &, File::Kind k);
	friend QDataStream &operator>>(QDataStream &, File::Kind &k);


	// struct Key
	// {
	// 	Key(Kind kind, QString name) : kind_(kind), name_(name) {}
	// 	Kind kind_ = Kind::none;
	// 	QString name_;
	// };


	struct LessFileSP
	{
		bool operator()(FileSP const& lhs, FileSP const& rhs) const
		{
			return std::tie(lhs->kind_, lhs->name_) < std::tie(rhs->kind_, rhs->name_);
		}

		bool operator()(FileSP const& lhs, FileID const& rhs) const
		{
			return std::tie(lhs->kind_, lhs->name_) < std::tie(rhs.kind_, rhs.name_);
		}

		bool operator()(FileID const& lhs, FileSP const& rhs) const
		{
			return std::tie(lhs.kind_, lhs.name_) < std::tie(rhs->kind_, rhs->name_);
		}

	};


private:
	QString hash_; // hash for file data if any (generated by server)
	QByteArray file_data_;

	QString local_file_name_; // local file name if any (could be empty)
};

using Files = std::set<FileSP, File::LessFileSP>;


class FileTableModel : public QAbstractTableModel
{
	Q_OBJECT
public:
	using QAbstractTableModel::QAbstractTableModel;

	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

	int rowCount(const QModelIndex &parent = QModelIndex()) const override;
	int	columnCount(const QModelIndex &parent = QModelIndex()) const override;
	Qt::ItemFlags flags(const QModelIndex &index) const override;

	QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
	bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;

	void update_from_task(Task const &task);

Q_SIGNALS:
	void rename_file(QString const &previous_value, QString const &s);

private:
	struct Row {
		QString name, path, kind;
	};

	std::vector<Row> rows_;
	bool editable_ = false;
};

} // namespace task
} // namespace ui
