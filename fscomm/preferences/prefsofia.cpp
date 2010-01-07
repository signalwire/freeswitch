#include <QtGui>
#include <fshost.h>
#include "prefsofia.h"

PrefSofia::PrefSofia(Ui::PrefDialog *ui, QObject *parent) :
        QObject(parent),
        _ui(ui)
{
    _settings = new QSettings();
}

void PrefSofia::readConfig()
{
    int guess_mask;
    char guess_ip[80];
    switch_find_local_ip(guess_ip, sizeof(guess_ip), &guess_mask, AF_INET);
    _ui->sofiaRtpIpEdit->setText(QString(guess_ip));
    _ui->sofiaSipIpEdit->setText(QString(guess_ip));
}

void PrefSofia::writeConfig()
{
    _settings->beginGroup("FreeSWITCH/conf");
    _settings->beginGroup("sofia.conf");

    /* General Settings */
    _settings->setValue("log-level", _ui->sofiaLogLevelSpin->value());
    _settings->setValue("auto-restart", _ui->sofiaAutoRestartCombo->currentText());
    _settings->setValue("debug-presence", _ui->sofiaDebugPresenceSpin->value());
    _settings->setValue("rewrite-multicasted-fs-path", _ui->sofiaRewriteMulticastedFsPathCombo->currentText());

    /* Profile settings */
    _settings->setValue("user-agent-string", _ui->sofiaUserAgentStringEdit->text());
    _settings->setValue("debug", _ui->sofiaDebugSpin->value());
    _settings->setValue("sip-trace", _ui->sofiaSipTraceCombo->currentText());
    _settings->setValue("context", _ui->sofiaContextEdit->text());
    _settings->setValue("rfc2833-pt", _ui->sofiaRfc2833PtEdit->text());
    _settings->setValue("sip-port", _ui->sofiaSipPortSpin->value());
    _settings->setValue("dialplan", _ui->sofiaDialplanEdit->text());
    _settings->setValue("dtmf-duration", _ui->sofiaDtmfDurationSpin->value());
    _settings->setValue("codec-prefs", _ui->sofiaCodecPrefsEdit->text());
    _settings->setValue("use-rtp-timer", _ui->sofiaUseRtpTimerCombo->currentText());
    _settings->setValue("rtp-timer-name", _ui->sofiaRtpTimerNameEdit->text());
    _settings->setValue("rtp-ip", _ui->sofiaRtpIpEdit->text());
    _settings->setValue("sip-ip", _ui->sofiaSipIpEdit->text());
    _settings->setValue("hold-music", _ui->sofiaHoldMusicEdit->text());
    _settings->setValue("apply-nat-acl", _ui->sofiaApplyNatAclEdit->text());
    _settings->setValue("manage-presence", _ui->sofiaManagePresenceCombo->currentText());
    _settings->setValue("max-proceeding", _ui->sofiaMaxProceedingEdit->text());
    _settings->setValue("inbound-codec-negotiation", _ui->sofiaInboundCodecNegotiationCombo->currentText());
    _settings->setValue("nonce-ttl", _ui->sofiaNonceTtlSpin->value());
    _settings->setValue("auth-calls", _ui->sofiaAuthCallsCombo->currentText());
    _settings->setValue("auth-all-packets", _ui->sofiaAuthAllPacketsCombo->currentText());
    _settings->setValue("ext-rtp-ip", _ui->sofiaExtRtpIpEdit->text());
    _settings->setValue("ext-sip-ip", _ui->sofiaExtSipIpEdit->text());
    _settings->setValue("rtp-timeout-sec", _ui->sofiaRtpTimeoutSecSpin->value());
    _settings->setValue("rtp-hold-timeout-sec", _ui->sofiaRtpHoldTimeoutSecSpin->value());
    _settings->setValue("disable-register", _ui->sofiaDisableRegisterCombo->currentText());
    _settings->setValue("challenge-realm", _ui->sofiaChallengeRealmCombo->currentText());


    _settings->endGroup();
    _settings->endGroup();
}
