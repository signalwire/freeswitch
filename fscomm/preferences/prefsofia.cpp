#include <QtGui>
#include <fshost.h>
#include "prefsofia.h"

PrefSofia::PrefSofia(Ui::PrefDialog *ui, QObject *parent) :
        QObject(parent),
        _ui(ui)
{
}

void PrefSofia::readConfig()
{
    ISettings *settings = new ISettings();
    QDomElement cfg = settings->getConfigNode("sofia.conf");
    if ( cfg.isNull() ) {
        qDebug() << "Issue a big warning!";
        return;
    }
    int guess_mask;
    char guess_ip[80];
    switch_find_local_ip(guess_ip, sizeof(guess_ip), &guess_mask, AF_INET);
    _ui->sofiaRtpIpEdit->setText(QString(guess_ip));
    _ui->sofiaSipIpEdit->setText(QString(guess_ip));

    /* General Settings */
    QDomNodeList l = cfg.elementsByTagName("global_settings");
    QDomNodeList global_params = l.at(0).toElement().elementsByTagName("param");
    for (int i = 0; i < global_params.count(); i++) {
        QDomElement el = global_params.at(i).toElement();
        if ( el.attribute("name") == "log-level" ) {
            _ui->sofiaLogLevelSpin->setValue(el.attribute("value").toInt());
        }
        if ( el.attribute("name") == "auto-restart") {
            _ui->sofiaAutoRestartCombo->setCurrentIndex(_ui->sofiaAutoRestartCombo->findText(el.attribute("value")));
        }
        if ( el.attribute("name") == "debug-presence") {
            _ui->sofiaDebugPresenceSpin->setValue(el.attribute("value").toInt());
        }
    }

    /* Profile settings */
    /* Get only the first settings, meaning one profile supported so far */
    QDomNodeList params = cfg.elementsByTagName("settings").at(0).toElement().elementsByTagName("param");
    for (int i = 0; i < params.count(); i++) {
        QDomElement el = params.at(i).toElement();
        if ( el.attribute("name") == "user-agent-string") {
            _ui->sofiaUserAgentStringEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "debug") {
            _ui->sofiaDebugSpin->setValue(el.attribute("value").toInt());
        }
        if ( el.attribute("name") == "sip-trace") {
            _ui->sofiaSipTraceCombo->setCurrentIndex(_ui->sofiaSipTraceCombo->findText(el.attribute("value")));
        }
        if ( el.attribute("name") == "context") {
            _ui->sofiaContextEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "rfc2833-pt") {
            _ui->sofiaRfc2833PtEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "sip-port") {
            _ui->sofiaSipPortSpin->setValue(el.attribute("value").toInt());
        }
        if ( el.attribute("name") == "dialplan") {
            _ui->sofiaDialplanEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "dtmf-duration") {
            _ui->sofiaDtmfDurationSpin->setValue(el.attribute("value").toInt());
        }
        if ( el.attribute("name") == "codec-prefs") {
            _ui->sofiaProfileCodecWidget->setCodecString(el.attribute("value"));
        }
        if ( el.attribute("name") == "use-rtp-timer") {
            _ui->sofiaUseRtpTimerCombo->setCurrentIndex(_ui->sofiaUseRtpTimerCombo->findText(el.attribute("value")));
        }
        if ( el.attribute("name") == "rtp-timer-name") {
            _ui->sofiaRtpTimerNameEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "rtp-ip") {
            _ui->sofiaRtpIpEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "sip-ip") {
            _ui->sofiaSipIpEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "hold-music") {
            _ui->sofiaHoldMusicEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "apply-nat-acl") {
            _ui->sofiaApplyNatAclEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "manage-presence") {
            _ui->sofiaManagePresenceCombo->setCurrentIndex(_ui->sofiaManagePresenceCombo->findText(el.attribute("value")));
        }
        if ( el.attribute("name") == "max-proceeding") {
            _ui->sofiaMaxProceedingEdit->setValue(el.attribute("value").toInt());
        }
        if ( el.attribute("name") == "inbound-codec-negotiation") {
            _ui->sofiaInboundCodecNegotiationCombo->setCurrentIndex(_ui->sofiaInboundCodecNegotiationCombo->findText(el.attribute("value")));
        }
        if ( el.attribute("name") == "nonce-ttl") {
            _ui->sofiaNonceTtlSpin->setValue(el.attribute("value").toInt());
        }
        if ( el.attribute("name") == "auth-calls") {
            _ui->sofiaAuthCallsCombo->setCurrentIndex(_ui->sofiaAuthCallsCombo->findText(el.attribute("value")));
        }
        if ( el.attribute("name") == "auth-all-packets") {
            _ui->sofiaAuthAllPacketsCombo->setCurrentIndex(_ui->sofiaAuthAllPacketsCombo->findText(el.attribute("value")));
        }
        if ( el.attribute("name") == "ext-sip-ip") {
            _ui->sofiaExtSipIpEdit->setText(el.attribute("value"));
        }
        if ( el.attribute("name") == "rtp-timeout-sec") {
            _ui->sofiaRtpTimeoutSecSpin->setValue(el.attribute("value").toInt());
        }
        if ( el.attribute("name") == "rtp-hold-timeout-sec") {
            _ui->sofiaRtpHoldTimeoutSecSpin->setValue(el.attribute("value").toInt());
        }
        if ( el.attribute("name") == "disable-register") {
            _ui->sofiaDisableRegisterCombo->setCurrentIndex(_ui->sofiaDisableRegisterCombo->findText(el.attribute("value")));
        }
        if ( el.attribute("name") == "challenge-realm") {
            _ui->sofiaChallengeRealmCombo->setCurrentIndex(_ui->sofiaChallengeRealmCombo->findText(el.attribute("value")));
        }
    }
    delete (settings);

}

void PrefSofia::postWriteConfig() {
    /* Here, we have to know if we need to restart the profile or not */
    return;
}

void PrefSofia::writeConfig()
{

    ISettings *settings = new ISettings(this);

    QDomElement e = settings->getConfigNode("sofia.conf");
    QDomNodeList nl = e.elementsByTagName("global_settings").at(0).toElement().elementsByTagName("param");
    /* General Settings */
    for (int i = 0; i < nl.count(); i++) {
        QDomElement el = nl.at(i).toElement();
        QDomAttr val = el.attributeNode("value");
        QDomAttr var = el.attributeNode("name");
        if ( var.value() == "log-level" ) {
            val.setValue(QString::number(_ui->sofiaLogLevelSpin->value()));
        }
        if ( var.value() == "auto-restart" ) {
            val.setValue(_ui->sofiaAutoRestartCombo->currentText());
        }
        if ( var.value() == "debug-presence" ) {
            val.setValue(QString::number(_ui->sofiaDebugPresenceSpin->value()));
        }
        if ( var.value() == "rewrite-multicasted-fs-path" ) {
            val.setValue(_ui->sofiaRewriteMulticastedFsPathCombo->currentText());
        }

    }
    /* Profile settings */
    /* Get only the first settings, meaning one profile supported so far */
    QDomNodeList params = e.elementsByTagName("settings").at(0).toElement().elementsByTagName("param");
    for (int i = 0; i < params.count(); i++) {
        QDomElement el = params.at(i).toElement();
        QDomAttr val = el.attributeNode("value");
        if ( el.attribute("name") == "user-agent-string") {
            val.setValue(_ui->sofiaUserAgentStringEdit->text());
        }
        if ( el.attribute("name") == "debug") {
            val.setValue(QString::number(_ui->sofiaDebugSpin->value()));
        }
        if ( el.attribute("name") == "sip-trace") {
            val.setValue(_ui->sofiaSipTraceCombo->currentText());
        }
        if ( el.attribute("name") == "context") {
            val.setValue(_ui->sofiaContextEdit->text());
        }
        if ( el.attribute("name") == "rfc2833-pt") {
            val.setValue(_ui->sofiaRfc2833PtEdit->text());
        }
        if ( el.attribute("name") == "sip-port") {
            val.setValue(QString::number(_ui->sofiaSipPortSpin->value()));
        }
        if ( el.attribute("name") == "dialplan") {
            val.setValue(_ui->sofiaDialplanEdit->text());
        }
        if ( el.attribute("name") == "dtmf-duration") {
            val.setValue(QString::number(_ui->sofiaDtmfDurationSpin->value()));
        }
        if ( el.attribute("name") == "codec-prefs") {
            val.setValue(_ui->sofiaProfileCodecWidget->getCodecString());
        }
        if ( el.attribute("name") == "use-rtp-timer") {
            val.setValue(_ui->sofiaUseRtpTimerCombo->currentText());
        }
        if ( el.attribute("name") == "rtp-timer-name") {
            val.setValue(_ui->sofiaRtpTimerNameEdit->text());
        }
        if ( el.attribute("name") == "rtp-ip") {
            val.setValue(_ui->sofiaRtpIpEdit->text());
        }
        if ( el.attribute("name") == "sip-ip") {
            val.setValue(_ui->sofiaSipIpEdit->text());
        }
        if ( el.attribute("name") == "hold-music") {
            val.setValue(_ui->sofiaHoldMusicEdit->text());
        }
        if ( el.attribute("name") == "apply-nat-acl") {
            val.setValue(_ui->sofiaApplyNatAclEdit->text());
        }
        if ( el.attribute("name") == "manage-presence") {
            val.setValue(_ui->sofiaManagePresenceCombo->currentText());
        }
        if ( el.attribute("name") == "max-proceeding") {
            val.setValue(_ui->sofiaMaxProceedingEdit->text());
        }
        if ( el.attribute("name") == "inbound-codec-negotiation") {
            val.setValue(_ui->sofiaInboundCodecNegotiationCombo->currentText());
        }
        if ( el.attribute("name") == "nonce-ttl") {
            val.setValue(QString::number(_ui->sofiaNonceTtlSpin->value()));
        }
        if ( el.attribute("name") == "auth-calls") {
            val.setValue(_ui->sofiaAuthCallsCombo->currentText());
        }
        if ( el.attribute("name") == "auth-all-packets") {
            val.setValue(_ui->sofiaAuthAllPacketsCombo->currentText());
        }
        if ( el.attribute("name") == "ext-rtp-ip") {
            val.setValue(_ui->sofiaExtRtpIpEdit->text());
        }
        if ( el.attribute("name") == "ext-sip-ip") {
            val.setValue(_ui->sofiaExtSipIpEdit->text());
        }
        if ( el.attribute("name") == "rtp-timeout-sec") {
            val.setValue(QString::number(_ui->sofiaRtpTimeoutSecSpin->value()));
        }
        if ( el.attribute("name") == "rtp-hold-timeout-sec") {
            val.setValue(QString::number(_ui->sofiaRtpHoldTimeoutSecSpin->value()));
        }
        if ( el.attribute("name") == "disable-register") {
            val.setValue(_ui->sofiaDisableRegisterCombo->currentText());
        }
        if ( el.attribute("name") == "challenge-realm") {
            val.setValue(_ui->sofiaChallengeRealmCombo->currentText());
        }
    }

    settings->setConfigNode(e, "sofia.conf");
    delete(settings);
}
