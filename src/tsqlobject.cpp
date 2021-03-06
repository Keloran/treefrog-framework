/* Copyright (c) 2010-2012, AOYAMA Kazuharu
 * All rights reserved.
 *
 * This software may be used and distributed according to the terms of
 * the New BSD License, which is incorporated herein by reference.
 */

#include <QtSql>
#include <QCoreApplication>
#include <QMetaObject>
#include <TSqlObject>
#include <TActionContext>
#include <TSqlQuery>
#include <TSystemGlobal>

#define REVISION_PROPERTY_NAME  "lock_revision"

/*!
  \class TSqlObject
  \brief The TSqlObject class is the base class of ORM objects.
  \sa TSqlORMapper
*/

/*!
  Constructor.
 */
TSqlObject::TSqlObject()
    : QObject(), QSqlRecord(), tblName(), sqlError()
{ }

/*!
  Copy constructor.
 */
TSqlObject::TSqlObject(const TSqlObject &other)
    : QObject(), QSqlRecord(*static_cast<const QSqlRecord *>(&other)),
      tblName(other.tblName), sqlError(other.sqlError)
{ }


TSqlObject &TSqlObject::operator=(const TSqlObject &other)
{
    QSqlRecord::operator=(*static_cast<const QSqlRecord *>(&other));
    tblName = other.tblName;
    sqlError = other.sqlError;
    return *this;
}

/*!
  Returns the table name, which is generated from the class name.
 */
QString TSqlObject::tableName() const
{
    if (tblName.isEmpty()) {
        QString clsname(metaObject()->className());
        for (int i = 0; i < clsname.length(); ++i) {
            if (i > 0 && clsname[i].isUpper()) {
                tblName += '_';
            }
            tblName += clsname[i].toLower();
        }
        tblName.remove(QRegExp("_object$"));
    }
    return tblName;
}

/*!
  \fn virtual int TSqlObject::primaryKeyIndex() const
  Returns the position of the primary key field for the table.
  This is a virtual function.
*/

/*!
  \fn virtual int TSqlObject::autoValueIndex() const
  Returns the position of the auto-generated value field for
  the table. This is a virtual function.
*/

/*!
  \fn bool TSqlObject::isNull() const
  Returns true if it is a null object, otherwise returns false.
*/

/*!
  \fn bool TSqlObject::isNew() const
  Returns true if it is a new object, otherwise returns false.
*/

/*!
  \fn QSqlError TSqlObject::error() const
  Returns a QSqlError object which contains information about
  the last error that occurred on the database.
*/

/*!
  Sets the \a record. Internal use. 
 */
void TSqlObject::setRecord(const QSqlRecord &record, const QSqlError &error)
{
    QSqlRecord::operator=(record);
    syncToObject();
    sqlError = error;
}

/*!
  Inserts this properties into the database.
 */
bool TSqlObject::create()
{
    // Sets the default value of 'revision' property
    int index = metaObject()->indexOfProperty(REVISION_PROPERTY_NAME);
    if (index >= 0) {
        setProperty(REVISION_PROPERTY_NAME, 1);  // 1 : default value
    }

    // Sets the values of 'created_at', 'updated_at' or 'modified_at' properties
    for (int i = metaObject()->propertyOffset(); i < metaObject()->propertyCount(); ++i) {
        const char *propName = metaObject()->property(i).name();
        if (QLatin1String("created_at") == propName || QLatin1String("updated_at") == propName
            || QLatin1String("modified_at") == propName) {
            setProperty(propName, QDateTime::currentDateTime());
        }
    }

    syncToSqlRecord();
    
    QString autoValName;
    QSqlRecord record = *this;
    if (autoValueIndex() >= 0) {
        autoValName = field(autoValueIndex()).name();
        record.remove(autoValueIndex()); // not insert the value of auto-value field
    }

    QSqlDatabase &database = TActionContext::current()->getDatabase(databaseId());
    QString ins = database.driver()->sqlStatement(QSqlDriver::InsertStatement, tableName(), record, false);
    if (ins.isEmpty()) {
        sqlError = QSqlError(QLatin1String("No fields to insert"),
                             QString(), QSqlError::StatementError);
        tWarn("SQL statement error, no fields to insert");
        return false;
    }

    QSqlQuery query(database);
    bool ret = query.exec(ins);
    tQueryLog("%s", qPrintable(ins));
    sqlError = query.lastError();
    if (!ret) {
        tSystemError("SQL insert error: %s", qPrintable(sqlError.text()));
    } else {
        // Gets the last inserted value of auto-value field
        if (autoValueIndex() >= 0) {
            QVariant lastid = query.lastInsertId();
            if (lastid.isValid()) {
                QObject::setProperty(autoValName.toLower().toLatin1().constData(), lastid);
            }
        }
    }
    return ret;
}

/*!
  Updates the record on the database with the primary key.
 */
bool TSqlObject::update()
{
    if (isNew()) {
        sqlError = QSqlError(QLatin1String("No record to update"),
                             QString(), QSqlError::UnknownError);
        tWarn("Unable to update the '%s' object. Create it before!", metaObject()->className());
        return false;
    }

    QSqlDatabase &database = TActionContext::current()->getDatabase(databaseId());
    QString where(" WHERE ");
    int revIndex = metaObject()->indexOfProperty(REVISION_PROPERTY_NAME);
    if (revIndex >= 0) {
        bool ok;
        int oldRevision = property(REVISION_PROPERTY_NAME).toInt(&ok);
        if (!ok || oldRevision <= 0) {
            sqlError = QSqlError(QLatin1String("Unable to convert the 'revision' property to an int"),
                                 QString(), QSqlError::UnknownError);
            tError("Unable to convert the 'revision' property to an int, %s", qPrintable(objectName()));
            return false;
        }

        setProperty(REVISION_PROPERTY_NAME, oldRevision + 1);
        
        where.append(TSqlQuery::escapeIdentifier(REVISION_PROPERTY_NAME, QSqlDriver::FieldName, database));
        where.append("=").append(TSqlQuery::formatValue(oldRevision, database));
        where.append(" AND ");
    }

    // Updates the value of 'updated_at' or 'modified_at'property
    for (int i = metaObject()->propertyOffset(); i < metaObject()->propertyCount(); ++i) {
        const char *propName = metaObject()->property(i).name();
        if (QLatin1String("updated_at") == propName || QLatin1String("modified_at") == propName) {
            setProperty(propName, QDateTime::currentDateTime());
            break;
        }
    }

    QString upd;   // UPDATE Statement
    upd.reserve(256);
    upd.append(QLatin1String("UPDATE ")).append(tableName()).append(QLatin1String(" SET "));

    for (int i = metaObject()->propertyOffset(); i < metaObject()->propertyCount(); ++i) {
        const char *propName = metaObject()->property(i).name();
        QVariant newval = QObject::property(propName);
        QVariant recval = QSqlRecord::value(QLatin1String(propName));
        if (recval.isValid() && recval != newval) {
            upd.append(TSqlQuery::escapeIdentifier(QLatin1String(propName), QSqlDriver::FieldName, database));
            upd.append(QLatin1Char('='));
            upd.append(TSqlQuery::formatValue(newval, database));
            upd.append(QLatin1String(", "));
        }
    }

    if (!upd.endsWith(QLatin1String(", "))) {
        tSystemDebug("SQL UPDATE: Same values as that of the record. No need to update.");
        return true;
    }

    upd.chop(2);
    syncToSqlRecord();
    
    const char *pkName = metaObject()->property(metaObject()->propertyOffset() + primaryKeyIndex()).name();
    if (primaryKeyIndex() < 0 || !pkName) {
        QString msg = QString("Not found the primary key for table ") + tableName();
        sqlError = QSqlError(msg, QString(), QSqlError::StatementError);
        tError("%s", qPrintable(msg));
        return false;
    }
    where.append(TSqlQuery::escapeIdentifier(pkName, QSqlDriver::FieldName, database));
    where.append("=").append(TSqlQuery::formatValue(property(pkName), database));
    upd.append(where);

    QSqlQuery query(database);
    bool res = query.exec(upd);
    tQueryLog("%s", qPrintable(upd));    
    sqlError = query.lastError();
    if (!res) {
        tSystemError("SQL update error: %s", qPrintable(sqlError.text()));
        return false;
    }
    
    // Optimistic lock check
    if (revIndex >= 0 && query.numRowsAffected() != 1) {
        QString msg = QString("Row was updated or deleted from table ") + tableName() + QLatin1String(" by another transaction");
        sqlError = QSqlError(msg, QString(), QSqlError::UnknownError);
        throw SqlException(msg, __FILE__, __LINE__);
    }
    return true;
}

/*!
  Deletes the record with this primary key from the database.
 */
bool TSqlObject::remove()
{
    syncToSqlRecord();

    QSqlDatabase &database = TActionContext::current()->getDatabase(databaseId());
    QString del = database.driver()->sqlStatement(QSqlDriver::DeleteStatement, tableName(), *static_cast<QSqlRecord *>(this), false);
    if (del.isEmpty()) {
        sqlError = QSqlError(QLatin1String("Unable to delete row"),
                             QString(), QSqlError::StatementError);
        return false;
    }

    del.append(" WHERE ");
    int revIndex = metaObject()->indexOfProperty(REVISION_PROPERTY_NAME);
    if (revIndex >= 0) {
        bool ok;
        int revsion = property(REVISION_PROPERTY_NAME).toInt(&ok);
        if (!ok || revsion <= 0) {
            sqlError = QSqlError(QLatin1String("Unable to convert the 'revision' property to an int"),
                                 QString(), QSqlError::UnknownError);
            tError("Unable to convert the 'revsion' property to an int, %s", qPrintable(objectName()));
            return false;
        }

        del.append(TSqlQuery::escapeIdentifier(REVISION_PROPERTY_NAME, QSqlDriver::FieldName, database));
        del.append("=").append(TSqlQuery::formatValue(revsion, database));
        del.append(" AND ");
    }

    const char *pkName = metaObject()->property(metaObject()->propertyOffset() + primaryKeyIndex()).name();
    if (primaryKeyIndex() < 0 || !pkName) {
        QString msg = QString("Not found the primary key for table ") + tableName();
        sqlError = QSqlError(msg, QString(), QSqlError::StatementError);
        tError("%s", qPrintable(msg));
        return false;
    }
    del.append(TSqlQuery::escapeIdentifier(pkName, QSqlDriver::FieldName, database));
    del.append("=").append(TSqlQuery::formatValue(property(pkName), database));

    QSqlQuery query(database);
    bool res = query.exec(del);
    tQueryLog("%s", qPrintable(del));
    sqlError = query.lastError();
    if (!res) {
        tSystemError("SQL delete error: %s", qPrintable(sqlError.text()));
        return false;
    }
    
    // Optimistic lock check
    if (query.numRowsAffected() != 1) {
        if (revIndex >= 0) {
            QString msg = QString("Row was updated or deleted from table ") + tableName() + QLatin1String(" by another transaction");
            sqlError = QSqlError(msg, QString(), QSqlError::UnknownError);
            throw SqlException(msg, __FILE__, __LINE__);
        }
        tWarn("Row was deleted by another transaction, %s", qPrintable(tableName()));
    }

    clear();
    return true;
}

/*!
  Reloads the values of  the record onto the properties.
 */
bool TSqlObject::reload()
{
    if (isEmpty()) {
        return false;
    }
    syncToObject();
    return true;
}

/*!
  Returns true if the values of the properties differ with the record on the
  database; otherwise returns false.
 */
bool TSqlObject::isModified() const
{
    if (isNew())
        return false;

    for (int i = 0; i < QSqlRecord::count(); ++i) {
        QString name = field(i).name().toLower();
        int index = metaObject()->indexOfProperty(name.toLatin1().constData());
        if (index >= 0) {
            if (value(name) != property(name.toLatin1().constData())) {
                return true;
            }
        }
    }
    return false;
}


void TSqlObject::syncToObject()
{
    int offset = metaObject()->propertyOffset();
    for (int i = 0; i < QSqlRecord::count(); ++i) {
        QString propertyName = field(i).name();
        int index = metaObject()->indexOfProperty(propertyName.toLower().toLatin1().constData());
        if (index >= offset) {
            QObject::setProperty(propertyName.toLower().toLatin1().constData(), value(propertyName));
        }
    }
}


void TSqlObject::syncToSqlRecord()
{
    QSqlRecord::operator=(TActionContext::current()->getDatabase(databaseId()).record(tableName()));
    const QMetaObject *metaObj = metaObject();
    for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i) {
        const char *propName = metaObj->property(i).name();
        int idx = indexOf(propName);
        if (idx >= 0) {
            QSqlRecord::setValue(idx, QObject::property(propName));
        } else {
            tWarn("invalid name: %s", propName);
        }
    }
}

/*!
  Returns a Hash object of the properties.
 */
QVariantHash TSqlObject::properties() const
{
    QVariantHash ret;
    const QMetaObject *metaObj = metaObject();
    for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i) {
        const char *propName = metaObj->property(i).name();
        QString n(propName);
        if (!n.isEmpty()) {
            ret.insert(n, QObject::property(propName));
        }
    }
    return ret;
}

/*!
  Set the \a values to the properties.
 */
void TSqlObject::setProperties(const QVariantHash &values)
{
    const QMetaObject *metaObj = metaObject();
    for (int i = metaObj->propertyOffset(); i < metaObj->propertyCount(); ++i) {
        const char *n = metaObj->property(i).name();
        QLatin1String key(n);
        if (values.contains(key)) {
            QObject::setProperty(n, values[key]);
        }
    }
}
