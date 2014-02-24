using FreeSWITCH;

namespace winFailToBan
{
    public class Fail2Ban : IApiPlugin , ILoadNotificationPlugin
    {
        public void Execute(ApiContext context)
        {
            var cmds = context.Arguments.Split(" ".ToCharArray());
            var cmd = cmds[0].ToLower();
            switch (cmd)
            {
                case "shutdown":
                    Shutdown();
                    break;

                default:
                    context.Stream.Write("\n\nInvalid Command\n\n");
                    break;
            }
        }

        public void ExecuteBackground(ApiBackgroundContext context)
        {
            return;
        }

        public static void Startup()
        {
            BanTracker.Startup();
            EventLoop.StartEvents();
        }

        public static void Shutdown()
        {
            EventLoop.StopEvents();
        }

        public bool Load()
        {
            Startup();
            return true;
        }
    }
}
