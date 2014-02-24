using System;
using FreeSWITCH;
using FreeSWITCH.Native;

namespace winFailToBan.Internal
{

    public class ConfigurationEventArgs : EventArgs
    {
        public SwitchXmlSearchBinding.XmlBindingArgs FsArgs { get; private set; }
        public fsConfigDocument Result { get; set; }
        public Boolean DontProcess { get; set; }

        public ConfigurationEventArgs(SwitchXmlSearchBinding.XmlBindingArgs args)
        {
            DontProcess = false;
            FsArgs = args;
            Result = null;
        }
    }
    
    // Bind XML search function turned into CLR events for ease use
    public class ConfigHandler : IDisposable
    {
        public event EventHandler<ConfigurationEventArgs> DirectoryRequest;
        public event EventHandler<ConfigurationEventArgs> DialPlanRequest;

        private IDisposable _binder; // object to bind to

        public void Dispose()
        {
            if(_binder != null)
                _binder.Dispose();
            _binder = null;
        }

        public String XmlCallback(SwitchXmlSearchBinding.XmlBindingArgs args)
        {
            String rv = null; // return value
            switch (args.Section.ToLower())
            {
                case "directory":
                    var dargs = new ConfigurationEventArgs(args);
                    if (DirectoryRequest != null)
                    {
                        var temp = DirectoryRequest;
                        temp(this, dargs);
                        if (dargs.DontProcess)
                            return null;
                        if (dargs.Result != null)
                            rv = dargs.Result.ToXMLString();
                    }
                    break;

                case "dialplan":
                    var dialargs = new ConfigurationEventArgs(args);
                    if(DialPlanRequest != null)
                    {
                        var temp = DialPlanRequest;
                        temp(this, dialargs);
                        if (dialargs.Result != null)
                            rv = dialargs.Result.ToXMLString();
                    }
                    break;
            }

            return rv ?? new fsNotFoundDocument().ToXMLString();
        }

        ~ConfigHandler()
        {
            Dispose();
        }

        public ConfigHandler()
        {
            _binder = SwitchXmlSearchBinding.Bind(XmlCallback,
                switch_xml_section_enum_t.SWITCH_XML_SECTION_DIRECTORY |
                switch_xml_section_enum_t.SWITCH_XML_SECTION_DIALPLAN);
            
        }
    }
}
