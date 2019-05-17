using System;
using System.Collections.Generic;
using System.Text;
using Microsoft.Deployment.WindowsInstaller;
using ICSharpCode.SharpZipLib.BZip2;
using System.IO;
using System.Net;

namespace Setup.CA.DownloadOpenH264
{
    public class CustomActions
    {
        [CustomAction]
        public static ActionResult DownloadOpenH264(Session session)
        {
            session.Log("Begin DownloadOpenH264");

            string filename = session.CustomActionData["location"] + @"openh264.dll";

            try
            {
                WebRequest request = HttpWebRequest.Create("http://ciscobinary.openh264.org/openh264-1.8.0-win64.dll.bz2");

                using (WebResponse response = request.GetResponse())
                {
                    Stream responseStream = response.GetResponseStream();
                    BZip2InputStream zisUncompressed = new BZip2InputStream(responseStream);
                    using (var output = File.Create(filename))
                    {
                        var buffer = new byte[2048];
                        int n;
                        while ((n = zisUncompressed.Read(buffer, 0, buffer.Length)) > 0)
                        {
                            output.Write(buffer, 0, n);
                        }
                    }
                }
            }
            catch {
                session.Log("Unable to download openh264 codec.");
            }

            return ActionResult.Success;
        }

        [CustomAction]
        public static ActionResult RemoveOpenH264Binary(Session session)
        {
            session.Log("Begin RemoveOpenH264Binary");
            string filename = session.CustomActionData["location"] + @"openh264.dll";

            try
            {
                // Check if file exists with its full path    
                if (File.Exists(filename))
                {
                    // If file found, delete it    
                    File.Delete(filename);
                    session.Log("RemoveOpenH264Binary deleted openh264.dll");
                }
            }
            catch (IOException ioExp)
            {
                session.Log("RemoveOpenH264Binary can't delete openh264.dll");
            }

            return ActionResult.Success;
        }
    }
}
