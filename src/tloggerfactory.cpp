/* Copyright (c) 2010-2012, AOYAMA Kazuharu
 * All rights reserved.
 *
 * This software may be used and distributed according to the terms of
 * the New BSD License, which is incorporated herein by reference.
 */

#include <QDir>
#include <QList>
#include <QHash>
#include <QPluginLoader>
#include <QMutex>
#include <QMutexLocker>
#include <TWebApplication>
#include <TLoggerPlugin>
#include "tloggerfactory.h"
#include "tfilelogger.h"

static QList<TLoggerInterface *> *ssifs = 0;
static QMutex mutex;

/*!
  \class TLoggerFactory
  \brief The TLoggerFactory class creates TLogger objects.
*/

static void cleanup()
{
    QMutexLocker locker(&mutex);

    if (ssifs)
        delete ssifs;
    ssifs = 0;
}


QStringList TLoggerFactory::keys()
{
    QMutexLocker locker(&mutex);

    loadPlugins();
    QStringList ret;
    ret << TFileLogger().key();

    for (QListIterator<TLoggerInterface *> i(*ssifs); i.hasNext(); ) {
        ret << i.next()->keys();
    }
    return ret;
}


TLogger *TLoggerFactory::create(const QString &key)
{
    QMutexLocker locker(&mutex);
 
    loadPlugins();
    TLogger *logger = 0;
    QString k = key.toLower();
    if (k == TFileLogger().key().toLower()) {
        logger = new TFileLogger;
    } else {
        for (QListIterator<TLoggerInterface *> i(*ssifs); i.hasNext(); ) {
            TLoggerInterface *lif = i.next();
            if (lif->keys().contains(k)) {
                logger = lif->create(k);
                break;
            }
        }
    }
    return logger;
}


void TLoggerFactory::loadPlugins()
{
    if (!ssifs) {
        ssifs = new QList<TLoggerInterface *>();
        qAddPostRoutine(cleanup);
        
        QDir dir(Tf::app()->pluginPath());
        QStringList list = dir.entryList(QDir::Files);
        for (QStringListIterator i(list); i.hasNext(); ) {
            QPluginLoader loader(dir.absoluteFilePath(i.next()));
            TLoggerInterface *iface = qobject_cast<TLoggerInterface *>(loader.instance());
            if ( iface ) {
                ssifs->append(iface);
            }
        }
    }
}



/*!
  \class TLoggerInterface
  \brief The TLoggerInterface class provides an interface to implement
  TLogger plugins.
*/


/*!
  \class TLoggerPlugin
  \brief The TLoggerPlugin class provides an abstract base for custom
  TLogger plugins.
*/
