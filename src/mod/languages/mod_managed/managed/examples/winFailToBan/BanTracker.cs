using System;
using System.Collections.Generic;
using System.Linq;
using FreeSWITCH;
using FreeSWITCH.Native;

namespace winFailToBan
{
    public static class BanTracker
    {
        public static int MaxFails = 3;
        public static int FailMinutes = 1;
        public static int BanMinutes = 1;
        public static String BanApi = @"system netsh adv fire add rule name={0} dir=in action=block remoteip={1}";
        public static String UnBanApi = @"system netsh adv fire delete rule name={0}";

        // Tracker object
        public static Dictionary<String, List<DateTime>> MainTracker =
            new Dictionary<string, List<DateTime>>();

        // Active Ban list Key=IP val=baninfo
        public static Dictionary<String, BanInfo> ActiveBans =
            new Dictionary<string, BanInfo>();


        public static void Startup()
        {
            LoadSettings();
        }

        private static void LoadSettings()
        {
            using (var a = new Api(null))
            {
                var setting = a.ExecuteString("global_getvar ban_maxfails");
                if (!String.IsNullOrEmpty(setting))
                    MaxFails = int.Parse(setting);

                setting = a.ExecuteString("global_getvar ban_failminutes");
                if (!String.IsNullOrEmpty(setting))
                    FailMinutes = int.Parse(setting);

                setting = a.ExecuteString("global_getvar ban_banminutes");
                if (!String.IsNullOrEmpty(setting))
                    BanMinutes = int.Parse(setting);
            }
        }

        private static void CleanOld(List<DateTime> l)
        {
            var expiretime = DateTime.Now.Subtract(new TimeSpan(0, FailMinutes, 0));
            var expired = l.Where(x => x < expiretime).ToList();
            expired.ForEach(x => l.Remove(x));
        }

        public static void CleanUp()
        {
            var templist = new List<String>();
            foreach (var kvp in MainTracker)
            {
                CleanOld(kvp.Value);
                if (kvp.Value.Count == 0)
                    templist.Add(kvp.Key);
            }

            templist.ForEach(i =>
            {
                MainTracker.Remove(i);
                Log.WriteLine(LogLevel.Critical, "FTB: Removed tracker entry for {0}", i);
            }); // remove all the dictinoary entries that are old

            templist.Clear();

            // now unban the expired bans
            templist.AddRange(from kvp in ActiveBans where kvp.Value.Expires < DateTime.Now select kvp.Key);
            templist.ForEach(Unban);
        }

        public static void TrackFailure(String ipAddress)
        {
            LoadSettings(); // just in case they've changed... 
            if (ActiveBans.ContainsKey(ipAddress))
                return; // don't process again, some delay may happen between the ban, and it taking effect by external system

            if (!MainTracker.ContainsKey(ipAddress))
                MainTracker.Add(ipAddress, new List<DateTime>());
            var l = MainTracker[ipAddress];
            CleanOld(l);  // clean out the old ones
            l.Add(DateTime.Now); // add the failure to the list
            Log.WriteLine(LogLevel.Critical, "Fail to ban logging attempt from {0} count is {1}", ipAddress, l.Count);
            if (l.Count > MaxFails)
            {
                // do the ban here
                l.Clear();
                MainTracker.Remove(ipAddress);
                Ban(ipAddress);
            }
        }

        public static void Ban(String ipAddress)
        {
            Log.WriteLine(LogLevel.Critical, "FTP Banning IP Address {0}", ipAddress);
            if (ActiveBans.ContainsKey(ipAddress))
                return; // it's already banned so f-it

            var bi = new BanInfo();
            ActiveBans.Add(ipAddress, bi);
            // Execute the ban API callback here
            var acmd = String.Format(BanApi, bi.FirewallRuleName, ipAddress);
            Log.WriteLine(LogLevel.Critical, "FTB: api command: {0}", acmd);
            using (var a = new Api(null))
                a.ExecuteString(acmd);
        }

        public static void Unban(String ipAddress)
        {
            Log.WriteLine(LogLevel.Critical, "FTB: Unbanning ip address {0}", ipAddress);
            if (!ActiveBans.ContainsKey(ipAddress))
                return; // nothing to do, it's not banned

            var bi = ActiveBans[ipAddress]; // get the ban info
            // Execute the unban API
            var acmd = String.Format(UnBanApi, bi.FirewallRuleName);
            Log.WriteLine(LogLevel.Critical, "FTB: api command: {0}", acmd);
            using (var a = new Api(null))
                a.ExecuteString(acmd);

            ActiveBans.Remove(ipAddress);
        }
    }

    public class BanInfo
    {
        public String FirewallRuleName { get; set; }
        public DateTime Expires { get; set; }

        public BanInfo()
        {
            FirewallRuleName = "ftb-" + Guid.NewGuid().ToString("N");
            Expires = DateTime.Now.AddMinutes(BanTracker.BanMinutes);
        }
    }
}
