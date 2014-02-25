using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using FreeSWITCH;
using winFailToBan.Internal;

namespace winFailToBan
{
    public class SampleApp : IAppPlugin
    {
        // example class for a dialplan APP just implment the run method
        public void Run(AppContext context)
        {
            var s = context.Session;
            var args = context.Arguments;
            // Do something with them here
        }
    }

    // Example class to implment an API command
    public class SampleApi : IApiPlugin
    {
        public void Execute(ApiContext context)
        {
            throw new NotImplementedException();
        }

        public void ExecuteBackground(ApiBackgroundContext context)
        {
            throw new NotImplementedException();
        }
    }

    // This examle class can be used to handle XML config lookups for dialplan and directory

    public class SampleConfigHandler : ILoadNotificationPlugin
    {
        private static ConfigHandler MyConfigHandler;

        static void HandleDirectoryLookups(Object sender, ConfigurationEventArgs e)
        {
            e.Result = null; // not found example just return after this
            // return;  // uncomment to just return not-fouond

            // return a directory object that will work
            var evt = e.FsArgs.Parameters; // Get the raw event that generated the userDir lookup
            var eventName = evt.GetHeader("Event-Name").value; // Find the event name

            // If your module handles voicemail authorization then implment the following
            // to update the voicemail password, when they change their voicemail password using TUI
            if (eventName == "CUSTOM")
            {
                var subClass = evt.GetValueOfHeader("Event-Subclass");
                if (subClass == "vm::maintenance")
                {
                    var vmaction = evt.GetValueOfHeader("VM-Actoun");
                    var username = evt.GetValueOfHeader("VM-User");
                    var newPassword = evt.GetValueOfHeader("VM-User-Password");
                    if (vmaction == "change-password" && !string.IsNullOrEmpty(username) &&
                        !String.IsNullOrEmpty(newPassword))
                    {
                        // implment your code to update the users vm password in your database
                        return; // No more processing we don't actually do an auth just a notification
                    }
                }
            }

            // to make sure we don't have some future events messing us up...
            if (eventName != "REQUEST_PARAMS" && eventName != "GENERAL")
                return;

            // implment the following if you want to handle gateway lookup from directory when a profile loads
            if (evt.GetValueOfHeader("purpose") == "gateways")
            {
                var profileName = evt.GetValueOfHeader("profile");
                // implment your gateway lookup
                //e.Result = new fsDomainGatewayDirectoryDocument(myGwStructure);
                return;
            }

            var action = evt.GetValueOfHeader("action", "none"); // get the action

            // If you want to handle ESL Logins implment the following
            if (action == "event_socket_auth")
            {
                // preform your stuff here
                // e.result = ...
                return;
            }

            // Normal lookup processing
            if (evt.GetHeader("user") == null || evt.GetHeader("domain") == null)
                return; // does't have required fields
            var method = evt.GetValueOfHeader("sip_auth_method", "unknown");
            var user = evt.GetValueOfHeader("user");
            var domain = evt.GetValueOfHeader("domain");

            // Some variables to return the params and variables section of the user record
            var variables = new Dictionary<String, String>();
            var uparams = new Dictionary<String, String>();


            // if you're implmenting reverse-auth of devices
            if (action == "reverse-auth-lookup")
            {
                // lookup stuff in your db
                uparams.Add("reverse-auth-user", "device uername");
                uparams.Add("reverse-auth-pass", "device password");
            }

            // if you handle voicemail passwords ...
            if (true /*check for voicemail box */)
            {
                uparams.Add("vm-password", "theirvmpassword");
                // the following is optional
                uparams.Add("MWI-Account", "registrationstring");
            }
            // add more parameters here
            uparams.Add("anyotherparameters", "value");

            // add variables here for example
            variables.Add("user_context", "theuserscontext");

            e.Result = new fsDirectoryDocument(
                domain,
                user,
                "theirpassword",
                uparams,
                variables);

            return;
        }

        // Example dialplan handler
        static void HandleDialPlanRequest(object sender, ConfigurationEventArgs e)
        {
            var evt = e.FsArgs.Parameters; // get the native event that caused this dialplan lookup

            // extract the minimum variables you will need
            var context = evt.GetValueOfHeader("Hunt-Context"); // the context
            var destination = evt.GetValueOfHeader("Hunt-Destination-Number"); // the dialed number or "DID"
            var ani = evt.GetValueOfHeader("Hunt-ANI"); // The ANI/CallerID number

            // A place to return the dialplan actions you want
            var actions = new List<String>(); // format is "app,data"

            // add the actions for your code they shouldn't be static this is just an example
            actions.Add("set,continue_on_fail=true");
            actions.Add("brige,sofia/mygateway/" + destination);
            actions.Add("transfer,fialedDest XML failedcontext");
            e.Result = new fsDialPlanDocument(context, actions);
            return; // Isn't this easy?
        }

        public bool Load()
        {
            // Start any threads doing event consumer loops
            MyConfigHandler = new ConfigHandler(); // init a config handler
            MyConfigHandler.DirectoryRequest += HandleDirectoryLookups;
            MyConfigHandler.DialPlanRequest += HandleDialPlanRequest;
            return true;
        }
    }

}
