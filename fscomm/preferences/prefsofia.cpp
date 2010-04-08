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
    QSettings settings;
    int guess_mask;
    char guess_ip[80];
    switch_find_local_ip(guess_ip, sizeof(guess_ip), &guess_mask, AF_INET);
    _ui->sofiaRtpIpEdit->setText(QString(guess_ip));
    _ui->sofiaSipIpEdit->setText(QString(guess_ip));

    settings.beginGroup("FreeSWITCH/conf");
    settings.beginGroup("sofia.conf");

    /* General Settings */
    settings.beginGroup("global_settings/params");
    _ui->sofiaLogLevelSpin->setValue(settings.value("log-level").toInt());
    _ui->sofiaAutoRestartCombo->setCurrentIndex(_ui->sofiaAutoRestartCombo->findText(settings.value("auto-restart").toString()));
    _ui->sofiaDebugPresenceSpin->setValue(settings.value("debug-presence").toInt());
    _ui->sofiaRewriteMulticastedFsPathCombo->setCurrentIndex(_ui->sofiaRewriteMulticastedFsPathCombo->findText(settings.value("rewrite-multicasted-fs-path").toString()));
    settings.endGroup();

    /* Profile settings */
    settings.beginGroup("profiles");
    settings.beginGroup("profile");


    settings.beginGroup("settings/params");
    _ui->sofiaUserAgentStringEdit->setText(settings.value("user-agent-string").toString());
    _ui->sofiaDebugSpin->setValue(settings.value("debug").toInt());
    _ui->sofiaSipTraceCombo->setCurrentIndex(_ui->sofiaSipTraceCombo->findText(settings.value("sip-trace").toString()));
    _ui->sofiaContextEdit->setText(settings.value("context").toString());
    _ui->sofiaRfc2833PtEdit->setText(settings.value("rfc2833-pt").toString());
    _ui->sofiaSipPortSpin->setValue(settings.value("sip-port").toInt());
    _ui->sofiaDialplanEdit->setText(settings.value("dialplan").toString());
    _ui->sofiaDtmfDurationSpin->setValue(settings.value("dtmf-duration").toInt());
    _ui->sofiaProfileCodecWidget->setCodecString(settings.value("codec-prefs").toString());
    _ui->sofiaUseRtpTimerCombo->setCurrentIndex(_ui->sofiaUseRtpTimerCombo->findText(settings.value("use-rtp-timer").toString()));
    _ui->sofiaRtpTimerNameEdit->setText(settings.value("rtp-timer-name").toString());
    _ui->sofiaRtpIpEdit->setText(settings.value("rtp-ip").toString());
    _ui->sofiaSipIpEdit->setText(settings.value("sip-ip").toString());
    _ui->sofiaHoldMusicEdit->setText(settings.value("hold-music").toString());
    _ui->sofiaApplyNatAclEdit->setText(settings.value("apply-nat-acl").toString());
    _ui->sofiaManagePresenceCombo->setCurrentIndex(_ui->sofiaManagePresenceCombo->findText(settings.value("manage-presence").toString()));
    _ui->sofiaMaxProceedingEdit->setValue(settings.value("max-proceeding").toInt());
    _ui->sofiaInboundCodecNegotiationCombo->setCurrentIndex(_ui->sofiaInboundCodecNegotiationCombo->findText(settings.value("inbound-codec-negotiation").toString()));
    _ui->sofiaNonceTtlSpin->setValue(settings.value("nonce-ttl").toInt());
    _ui->sofiaAuthCallsCombo->setCurrentIndex(_ui->sofiaAuthCallsCombo->findText(settings.value("auth-calls").toString()));
    _ui->sofiaAuthAllPacketsCombo->setCurrentIndex(_ui->sofiaAuthAllPacketsCombo->findText(settings.value("auth-all-packets").toString()));
    _ui->sofiaExtRtpIpEdit->setText(settings.value("ext-rtp-ip").toString());
    _ui->sofiaExtSipIpEdit->setText(settings.value("ext-sip-ip").toString());
    _ui->sofiaRtpTimeoutSecSpin->setValue(settings.value("rtp-timeout-sec").toInt());
    _ui->sofiaRtpHoldTimeoutSecSpin->setValue(settings.value("rtp-hold-timeout-sec").toInt());
    _ui->sofiaDisableRegisterCombo->setCurrentIndex(_ui->sofiaDisableRegisterCombo->findText(settings.value("disable-register").toString()));
    _ui->sofiaChallengeRealmCombo->setCurrentIndex(_ui->sofiaChallengeRealmCombo->findText(settings.value("challenge-realm").toString()));
    settings.endGroup();

    settings.endGroup();
    settings.endGroup();
    settings.endGroup();
    settings.endGroup();

}

void PrefSofia::writeConfig()
{
    QSettings settings;
    settings.beginGroup("FreeSWITCH/conf");
    settings.beginGroup("sofia.conf");

    /* General Settings */
    settings.beginGroup("global_settings/params");
    settings.setValue("log-level", _ui->sofiaLogLevelSpin->value());
    settings.setValue("auto-restart", _ui->sofiaAutoRestartCombo->currentText());
    settings.setValue("debug-presence", _ui->sofiaDebugPresenceSpin->value());
    settings.setValue("rewrite-multicasted-fs-path", _ui->sofiaRewriteMulticastedFsPathCombo->currentText());
    settings.endGroup();

    /* Profile settings */
    settings.beginGroup("profiles");
    settings.beginGroup("profile");

    settings.beginGroup("attrs");
    settings.setValue("name", "softphone");
    settings.endGroup();

    settings.beginGroup("settings/params");
    settings.setValue("user-agent-string", _ui->sofiaUserAgentStringEdit->text());
    settings.setValue("debug", _ui->sofiaDebugSpin->value());
    settings.setValue("sip-trace", _ui->sofiaSipTraceCombo->currentText());
    settings.setValue("context", _ui->sofiaContextEdit->text());
    settings.setValue("rfc2833-pt", _ui->sofiaRfc2833PtEdit->text());
    settings.setValue("sip-port", _ui->sofiaSipPortSpin->value());
    settings.setValue("dialplan", _ui->sofiaDialplanEdit->text());
    settings.setValue("dtmf-duration", _ui->sofiaDtmfDurationSpin->value());
    settings.setValue("codec-prefs", _ui->sofiaProfileCodecWidget->getCodecString());
    settings.setValue("use-rtp-timer", _ui->sofiaUseRtpTimerCombo->currentText());
    settings.setValue("rtp-timer-name", _ui->sofiaRtpTimerNameEdit->text());
    settings.setValue("rtp-ip", _ui->sofiaRtpIpEdit->text());
    settings.setValue("sip-ip", _ui->sofiaSipIpEdit->text());
    settings.setValue("hold-music", _ui->sofiaHoldMusicEdit->text());
    settings.setValue("apply-nat-acl", _ui->sofiaApplyNatAclEdit->text());
    settings.setValue("manage-presence", _ui->sofiaManagePresenceCombo->currentText());
    settings.setValue("max-proceeding", _ui->sofiaMaxProceedingEdit->text());
    settings.setValue("inbound-codec-negotiation", _ui->sofiaInboundCodecNegotiationCombo->currentText());
    settings.setValue("nonce-ttl", _ui->sofiaNonceTtlSpin->value());
    settings.setValue("auth-calls", _ui->sofiaAuthCallsCombo->currentText());
    settings.setValue("auth-all-packets", _ui->sofiaAuthAllPacketsCombo->currentText());
    settings.setValue("ext-rtp-ip", _ui->sofiaExtRtpIpEdit->text());
    settings.setValue("ext-sip-ip", _ui->sofiaExtSipIpEdit->text());
    settings.setValue("rtp-timeout-sec", _ui->sofiaRtpTimeoutSecSpin->value());
    settings.setValue("rtp-hold-timeout-sec", _ui->sofiaRtpHoldTimeoutSecSpin->value());
    settings.setValue("disable-register", _ui->sofiaDisableRegisterCombo->currentText());
    settings.setValue("challenge-realm", _ui->sofiaChallengeRealmCombo->currentText());
    settings.endGroup();

    settings.endGroup();
    settings.endGroup();
    settings.endGroup();
    settings.endGroup();
}
