MRCP/2.0 865 DEFINE-GRAMMAR 543257
Channel-Identifier:32AECB23433801@speechrecog
Content-Type:application/srgs+xml
Content-ID:<request1@form-level.store>
Content-Length:685

<?xml version="1.0"?>

<!-- the default grammar language is US English -->
<grammar xmlns="http://www.w3.org/2001/06/grammar"
        xml:lang="en-US" version="1.0">

<!-- single language attachment to tokens -->
	<rule id="yes">
           <one-of>
                 <item xml:lang="fr-CA">oui</item>
                 <item xml:lang="en-US">yes</item>
           </one-of>
     </rule>

<!-- single language attachment to a rule expansion -->
     <rule id="request">
           may I speak to
           <one-of xml:lang="fr-CA">
                 <item>Michel Tremblay</item>
                 <item>Andre Roy</item>
           </one-of>
     </rule>

</grammar>