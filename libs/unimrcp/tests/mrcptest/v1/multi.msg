//speechsynth
GET-PARAMS 543256 MRCP/1.0
Voice-gender:
Voice-variant:

SET-PARAMS 543256 MRCP/1.0
Voice-gender:female
Voice-variant:3

SPEAK 543257 MRCP/1.0
Kill-On-Barge-In:false
Voice-gender:neutral
Prosody-volume:medium
Content-Type:application/synthesis+ssml
Content-Length:412

<?xml version="1.0"?>
<speak>
<paragraph>
       <sentence>You have 4 new messages.</sentence>
       <sentence>The first is from <say-as type="name">Stephanie Williams</say-as> 
       <mark name="Stephanie"/> and arrived at <break/>
       <say-as type="time">3:45pm</say-as>.</sentence>
       
       <sentence>The subject is <prosody rate="-20%">ski trip</prosody></sentence>
</paragraph>
</speak>