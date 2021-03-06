#ifndef TACTIONVIEW_H
#define TACTIONVIEW_H

#include <QObject>
#include <QHash>
#include <QTextStream>
#include <QVariant>
#include <TGlobal>
#include <TActionController>
#include <TActionHelper>
#include <TViewHelper>
#include <TPrototypeAjaxHelper>
#include <THttpUtility>


class T_CORE_EXPORT TActionView : public QObject, public TActionHelper, public TViewHelper, public TPrototypeAjaxHelper
{
public:
    TActionView();
    virtual ~TActionView() { }

    virtual QString toString() = 0;
    QString yield() const;
    QString renderPartial(const QString &templateName, const QVariantHash &vars = QVariantHash()) const;
    QString authenticityToken() const;
    QVariant variant(const QString &name) const;
    bool hasVariant(const QString &name) const;
    const TActionController *controller() const;
    const THttpRequest &httpRequest() const;

protected:
    QString echo(const QString &str);
    QString echo(const char *str);
    QString echo(const QByteArray &str);
    QString echo(int n, int base = 10);
    QString echo(double d, char format = 'g', int precision = 6);
    QString echo(const THtmlAttribute &attr);
    QString echo(const QVariant &var);
    QString eh(const QString &str);
    QString eh(const char *str);
    QString eh(const QByteArray &str);
    QString eh(int n, int base = 10);
    QString eh(double d, char format = 'g', int precision = 6);
    QString eh(const THtmlAttribute &attr);
    QString eh(const QVariant &var);
    QString responsebody;

private:
    Q_DISABLE_COPY(TActionView)

    void setVariantHash(const QVariantHash &vars);
    void setController(TActionController *controller);
    void setSubActionView(TActionView *actionView);
    virtual const TActionView *actionView() const { return this; }

    TActionController *actionController;
    TActionView *subView;
    QVariantHash variantHash;

    friend class TActionController;
    friend class TActionMailer;
    friend class TDirectView;
};


inline void TActionView::setSubActionView(TActionView *actionView)
{
    subView = actionView;
}

inline const TActionController *TActionView::controller() const
{
    return actionController;
}

inline const THttpRequest &TActionView::httpRequest() const
{
    return controller()->httpRequest();
}

inline void TActionView::setVariantHash(const QVariantHash &vars)
{
    variantHash = vars;
}

inline QVariant TActionView::variant(const QString &name) const
{
    return variantHash.value(name);
}

inline bool TActionView::hasVariant(const QString &name) const
{
    return variantHash.contains(name);
}

inline QString TActionView::echo(const QString &str)
{
    responsebody += str;
    return QString();
}

inline QString TActionView::echo(const char *str)
{
    responsebody += QString(str);  // using codecForCStrings()
    return QString();
}

inline QString TActionView::echo(const QByteArray &str)
{
    responsebody += QString(str);  // using codecForCStrings()
    return QString();
}

inline QString TActionView::echo(int n, int base)
{
    responsebody += QString::number(n, base);
    return QString();  
}

inline QString TActionView::echo(double d, char format, int precision)
{
    responsebody += QString::number(d, format, precision);
    return QString(); 
}

inline QString TActionView::echo(const QVariant &var)
{
    responsebody += var.toString();
    return QString();
}

inline QString TActionView::eh(const QString &str)
{
    return echo(THttpUtility::htmlEscape(str));
}

inline QString TActionView::eh(const char *str)
{
    return echo(THttpUtility::htmlEscape(str));
}

inline QString TActionView::eh(const QByteArray &str)
{
    return echo(THttpUtility::htmlEscape(str));
}

inline QString TActionView::eh(int n, int base)
{
    return echo(THttpUtility::htmlEscape(QString::number(n, base)));
}

inline QString TActionView::eh(double d, char format, int precision)
{
    return echo(THttpUtility::htmlEscape(QString::number(d, format, precision)));
}

inline QString TActionView::eh(const QVariant &var)
{
    return echo(THttpUtility::htmlEscape(var.toString()));
}

inline void TActionView::setController(TActionController *controller)
{
    actionController = controller;
}

#endif // TACTIONVIEW_H
