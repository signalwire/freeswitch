all: $(MODNAME).so 

$(MODNAME).so:
	/usr/local/freeswitch/bin/gmcs -debug -unsafe -t:library -keyfile:public.snk \
	-out:FreeSwitch.NET.dll -reference:Mono.Posix.dll \
	Properties/AssemblyInfo.cs \
	Marshaling/Types/ApiInterfaceMarshal.cs \
	Marshaling/Types/ApplicationInterfaceMarshal.cs \
	Marshaling/Types/BufferMarshal.cs \
	Marshaling/Types/CallerExtensionMarshal.cs \
	Marshaling/Types/CallerProfileMarshal.cs \
	Marshaling/Types/ChannelFlagMarshal.cs \
	Marshaling/Types/ChannelMarshal.cs \
	Marshaling/Types/ChannelStateMarshal.cs \
	Marshaling/Types/ChannelTimetableMarshal.cs \
	Marshaling/Types/CodecImplementationMarshal.cs \
	Marshaling/Types/CodecInterfaceMarshal.cs \
	Marshaling/Types/CodecMarshal.cs \
	Marshaling/Types/CodecTypeMarshal.cs \
	Marshaling/Types/CoreSessionMarshal.cs \
	Marshaling/Types/EventMarshal.cs \
	Marshaling/Types/FileHandleMarshal.cs \
	Marshaling/Types/FrameMarshal.cs \
	Marshaling/Types/IOEventHooksMarshal.cs \
	Marshaling/Types/LoadableModuleInterfaceMarshal.cs \
	Marshaling/Types/LoadableModuleMarshal.cs \
	Marshaling/Types/SpeechHandleMarshal.cs \
	Marshaling/Types/StateHandlerTableMarshal.cs \
	Marshaling/Types/StreamHandleMarshal.cs \
	Marshaling/Types/TypesMarshal.cs \
	Marshaling/Types/TimerMarshal.cs \
	Marshaling/BufferMarshaler.cs \
	Marshaling/CallerExtensionMarshaler.cs \
	Marshaling/CallerProfileMarshaler.cs \
	Marshaling/ChannelMarshaler.cs \
	Marshaling/ChannelTimetableMarshaler.cs \
	Marshaling/CodecMarshaler.cs \
	Marshaling/CoreSessionMarshaler.cs \
	Marshaling/EventMarshaler.cs \
	Marshaling/FileHandleMarshaler.cs \
	Marshaling/MemoryPoolMarshaler.cs \
	Marshaling/SpeechHandleMarshaler.cs \
	Marshaling/StateHandlerTableMarshaler.cs \
	Marshaling/StreamHandleMarshaler.cs \
	Marshaling/TimerMarshaler.cs \
	Modules/Api.cs \
	Modules/Application.cs \
	Switch/CallerProfile.cs \
	Switch/Channel.cs \
	Switch/Console.cs \
	Switch/CoreSession.cs \
	Switch/Event.cs \
	Switch/Ivr.cs \
	Switch/LoadableModule.cs \
	Switch/Log.cs \
	Switch/StreamHandle.cs \
	Types/ApiFunction.cs \
	Types/ApplicationFunction.cs \
	Types/ApplicationInterface.cs \
	Types/Buffer.cs \
	Types/CallCause.cs \
	Types/CallerExtension.cs \
	Types/CallerProfile.cs \
	Types/Channel.cs \
	Types/ChannelFlag.cs \
	Types/ChannelState.cs \
	Types/ChannelTimetable.cs \
	Types/Codec.cs \
	Types/CoreSession.cs \
	Types/DtmfCallbackFunction.cs \
	Types/InputCallbackFunction.cs \
	Types/InputType.cs \
	Types/Event.cs \
	Types/EventCallback.cs \
	Types/EventType.cs \
	Types/FileHandle.cs \
	Types/LoadableModule.cs \
	Types/LoadableModuleInterface.cs \
	Types/LogLevel.cs \
	Types/MemoryPool.cs \
	Types/Module.cs \
	Types/Status.cs \
	Types/SpeechHandle.cs \
	Types/StateHandlerTable.cs \
	Types/StreamHandle.cs \
	Types/TextChannel.cs \
	Types/Timer.cs \
	Common.cs \
	Module.cs \
	Ivr.cs \
	Log.cs \
	
clean:
	rm -fr *.dll

install:
	cp -f FreeSwitch.NET.dll /usr/local/freeswitch/lib/
