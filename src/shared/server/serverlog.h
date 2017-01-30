#ifndef DP_SRV_SERVERLOG_H
#define DP_SRV_SERVERLOG_H

#include <QDateTime>
#include <QUuid>
#include <QHostAddress>

class QJsonObject;

namespace server {

/**
 * @brief Server log entry
 */
class Log {
	Q_GADGET
public:
	enum class Level {
		Error, // severe message (requires admin attention)
		Warn,  // acceptable errors
		Info,  // useful info for moderators
		Debug, // useful info for developers
	};
	Q_ENUM(Level)

	enum class Topic {
		Join,      // user joined a session
		Leave,     // user left a session
		Kick,      // user was kicked
		Ban,       // user was banned
		Unban,     // a ban was lifted
		Op,        // user was granted OP
		Deop,      // OP status was removed
		BadData,   // Received an invalid message from a client
		RuleBreak, // User tried to use a command they're not allowed to
		PubList,   // Session announcement
		Status     // General stuff
	};
	Q_ENUM(Topic)

	Log() : m_timestamp(QDateTime::currentDateTime()), m_level(Level::Warn), m_topic(Topic::Status) { }
	Log(const QDateTime &ts, const QUuid &session, const QString &user, Level level, Topic topic, const QString message)
		: m_timestamp(ts), m_session(session), m_user(user), m_level(level), m_topic(topic), m_message(message)
		{ }

	//! Get the entry timestamp
	QDateTime timestamp() const { return m_timestamp; }

	//! Get the session ID (null if not pertinent to any session)
	QUuid session() const { return m_session; }

	//! Get the user info triplet (ID;IP;name) or empty if not pertinent to any user
	QString user() const { return m_user; }

	//! Get the log entry severity level
	Level level() const { return m_level; }

	//! What's this log entry about?
	Topic topic() const { return m_topic; }

	Log &about(Level l, Topic t) { m_level=l; m_topic=t; return *this; }
	Log &user(uint8_t id, const QHostAddress &ip, const QString &name) { m_user = QStringLiteral("%1;%2;%3").arg(int(id)).arg(ip.toString()).arg(name); return *this; }
	Log &session(const QUuid &id) { m_session=id; return *this; }
	Log &message(const QString &msg) { m_message=msg; return *this; }

	/**
	 * @brief Get the log message as a string
	 * @param abridged if true, the timestamp and log level are omitted
	 */
	QString toString(bool abridged=false) const;

	/**
	 * @brief Get the log message as a JSON object
	 *
	 * If noPrivateData is true, this may return a blank object if the whole
	 * log entry contains information only the server administrator should see.
	 *
	 * @param noPrivateData if true, private data (user IP address) is omitted
	 */
	QJsonObject toJson(bool noPrivateData=false) const;

private:
	QDateTime m_timestamp;
	QUuid m_session;
	QString m_user;
	Level m_level;
	Topic m_topic;
	QString m_message;
};

class ServerLog;

/**
 * @brief Log query builder
 */
class ServerLogQuery {
public:
	ServerLogQuery(const ServerLog &log) : m_log(log), m_offset(0), m_limit(0) { }

	ServerLogQuery &session(const QUuid &id) { m_session = id; return *this; }
	ServerLogQuery &page(int page, int entriesPerPage) { m_offset = page*entriesPerPage; m_limit=entriesPerPage; return *this; }
	ServerLogQuery &after(const QDateTime &ts) { m_after = ts; return *this; }

	bool isFiltered() const { return !m_session.isNull() || m_offset>0 || m_limit>0; }
	QList<Log> get() const;

private:
	const ServerLog &m_log;
	QUuid m_session;
	int m_offset;
	int m_limit;
	QDateTime m_after;
};

/**
 * @brief Abstract base class for server logger implementations
 */
class ServerLog
{
public:
	ServerLog() : m_silent(false) { }
	virtual ~ServerLog() = default;

	//! Don't log messages to stderr
	void setSilent(bool silent) { m_silent = silent; }

	/**
	 * @brief Log a message
	 *
	 * @param entry
	 */
	void logMessage(const Log &entry);

	/**
	 * @brief Get all available log messages that match the given filters
	 *
	 * @param session get only log entries for this session
	 * @param after get messages whose timestamp is greater than this
	 * @param offset ignore first *offset* messages
	 * @param limit return at most this many messages
	 */
	virtual QList<Log> getLogEntries(const QUuid &session, const QDateTime &after, int offset, int limit) const = 0;

	/**
	 * @brief Return a query builder
	 * @return
	 */
	ServerLogQuery query() const { return ServerLogQuery(*this); }

protected:
	virtual void storeMessage(const Log &entry) = 0;

private:
	bool m_silent;
};

inline QList<Log> ServerLogQuery::get() const {
	return m_log.getLogEntries(m_session, m_after, m_offset, m_limit);
}

/**
 * @brief A simple ServerLog implementation that keeps the latest messages in memory
 */
class InMemoryLog : public ServerLog
{
public:
	InMemoryLog() : m_limit(1000) { }
	void setHistoryLimit(int limit);

	QList<Log> getLogEntries(const QUuid &session, const QDateTime &after, int offset, int limit) const override;

protected:
	void storeMessage(const Log &entry) override;

private:
	QList<Log> m_history;
	int m_limit;
};

}

#endif
