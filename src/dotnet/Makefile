all: $(MODNAME).so 

$(MODNAME).so:
	gmcs -debug -unsafe -t:library -keyfile:public.snk \
	-out:Freeswitch.dll -reference:Mono.Posix.dll \
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
	Marshaling/Types/TypesMarshal.cs \
	Marshaling/BufferMarshaler.cs \
	Marshaling/CallerExtensionMarshaler.cs \
	Marshaling/CallerProfileMarshaler.cs \
	Marshaling/ChannelMarshaler.cs \
	Marshaling/ChannelTimetableMarshaler.cs \
	Marshaling/CoreSessionMarshaler.cs \
	Marshaling/EventMarshaler.cs \
	Marshaling/FileHandleMarshaler.cs \
	Marshaling/MemoryPoolMarshaler.cs \
	Marshaling/StreamHandleMarshaler.cs \
	Modules/Api.cs \
	Modules/Application.cs \
	Switch/CallerProfile.cs \
	Switch/Channel.cs \
	Switch/Console.cs \
	Switch/CoreSession.cs \
	Switch/Ivr.cs \
	Switch/LoadableModule.cs \
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
	Types/CoreSession.cs \
	Types/DtmfCallbackFunction.cs \
	Types/Event.cs \
	Types/FileHandle.cs \
	Types/LoadableModule.cs \
	Types/LoadableModuleInterface.cs \
	Types/MemoryPool.cs \
	Types/Module.cs \
	Types/Status.cs \
	Types/StreamHandle.cs \
	Types/TextChannel.cs \
	Common.cs \
	Core.cs \
	Ivr.cs \
	Module.cs \
	
clean:
	rm -fr *.dll

install:
	cp -f Freeswitch.dll /usr/local/freeswitch/lib/
