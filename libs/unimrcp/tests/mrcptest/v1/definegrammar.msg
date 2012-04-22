//speechrecog
DEFINE-GRAMMAR 543257 MRCP/1.0
Content-Type:application/grammar+xml
Content-Id:request1@form-level.store
Content-Length:309

<?xml version="1.0"?>

<!-- the default grammar language is US English -->
<grammar xml:lang="en-US" version="1.0">

<!-- single language attachment to tokens -->
<rule id="yes">
    <one-of>
        <item xml:lang="fr-CA">oui</item>
        <item xml:lang="en-US">yes</item>
     </one-of>
</rule>