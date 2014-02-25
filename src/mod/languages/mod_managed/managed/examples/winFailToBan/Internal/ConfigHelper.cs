using System;
using System.Collections.Generic;
using System.IO;
using System.Linq;
using System.Xml.Linq;
using FreeSWITCH.Native;
using Stream = System.IO.Stream;

namespace winFailToBan.Internal
{
    public abstract class fsConfigDocument
    {
        public XElement xmldoc;
        public XElement main;

        protected fsConfigDocument(String SectionName)
        {
            main = new XElement("wtf",
                                  new XElement("document",
                                               new XAttribute("type", "freeswitch/xml"),
                                               new XElement("section",
                                                            new XAttribute("name", SectionName),
                                                            new XAttribute("description", "Auto generated")
                                                   )));
            xmldoc = main.Descendants("document").Single();
        }

        public Int64 Length()
        {
            return xmldoc.ToString().Length + 2;
        }

        public String ToXMLString()
        {
            return xmldoc.ToString();
        }

        public Int64 WriteXML(Stream outStream)
        {
            var wr = new StreamWriter(outStream);
            wr.WriteLine(xmldoc.ToString());
            wr.Flush();
            return xmldoc.ToString().Length + 2;
        }

        public void WriteXML(TextWriter outText)
        {
            outText.Write(xmldoc.ToString());
        }


        protected void SectionChild(XElement Node)
        {
            // ReSharper disable PossibleNullReferenceException
            if (xmldoc != null) xmldoc.Element("section").Add(Node);
            // ReSharper restore PossibleNullReferenceException
        }
    }

    public class fsNotFoundDocument : fsConfigDocument
    {
        public fsNotFoundDocument()
            : base("result")
        {
            SectionChild(new XElement("result",
                                      new XAttribute("status", "not found")));
        }
    }

    public class fsDomainGatewayDirectoryDocument : fsConfigDocument
    {
        public fsDomainGatewayDirectoryDocument(Dictionary<String, Dictionary<String, Dictionary<String, String>>> domainGatewayList)
            : base("directory")
        {
            if (xmldoc == null)
                return;
            foreach (var v in
                    (from d in domainGatewayList
                     select new XElement("domain",
                                         new XAttribute("name", d.Key),
                                         new XElement("user",
                                             new XAttribute("id", "gatewaydummyser"),
                                               new XElement("gateways",
                                                      from g in d.Value
                                                      select new XElement("gateway",
                                                                          new XAttribute("name", g.Key),
                                                                          from p in g.Value
                                                                          select new XElement("param",
                                                                                              new XAttribute(
                                                                                                  "name", p.Key),
                                                                                              new XAttribute(
                                                                                                  "value", p.Value))))))))
                xmldoc.Element("section").Add(v);
        }
    }

    public class fsDirectoryDocument : fsConfigDocument
    {
        public fsDirectoryDocument(String Domain, String User, String Password)
            : base("directory")
        {
            SectionChild(new XElement("domain",
                                      new XAttribute("name", Domain),
                                      new XElement("user",
                                                   new XAttribute("id", User),
                                                   new XElement("params",
                                                                new XElement("param",
                                                                             new XAttribute("name", "password"),
                                                                             new XAttribute("value", Password)
                                                                    )
                                                       )
                                          )
                             ));
        }
        public fsDirectoryDocument(String Domain, String User, String Password, Dictionary<String, String> Params)
            : this(Domain, User, Password)
        {
            if (Params == null)
                return;
            // ReSharper disable PossibleNullReferenceException
            xmldoc.Element("section").Element("domain").Element("user").Element("params").Add(
                // ReSharper restore PossibleNullReferenceException
                from par in Params
                select new XElement("param",
                                    new XAttribute("name", par.Key),
                                    new XAttribute("value", par.Value)));
        }

        public fsDirectoryDocument(String Domain, String User, String Password, Dictionary<String, String> Params, Dictionary<String, String> Variables)
            : this(Domain, User, Password, Params)
        {
            if (Variables == null)
                return;
            // ReSharper disable PossibleNullReferenceException
            xmldoc.Element("section").Element("domain").Element("user").Add(
                // ReSharper restore PossibleNullReferenceException
                new XElement("variables", from v in Variables
                                          select new XElement("variable",
                                                              new XAttribute("name", v.Key),
                                                              new XAttribute("value", v.Value))));
        }
    }

    public class fsDialPlanDocument : fsConfigDocument
    {

        private static XElement MakeActionNode(String ActionString)
        {
            var p = ActionString.Split(",".ToCharArray(), 2);
            var rv = new XElement("action",
                                  new XAttribute("application", p[0]));
            if (p.Length > 1)
                rv.Add(new XAttribute("data", p[1]));
            return rv;
        }

        public fsDialPlanDocument(String Context, IEnumerable<String> Actions)
            : base("dialplan")
        {
            SectionChild(new XElement("context",
                                      new XAttribute("name", Context),
                                      new XElement("extension",
                                                   new XAttribute("name", "extension"),
                                                   new XElement("condition",
                                                                from act in Actions
                                                                select MakeActionNode(act)))));
        }
    }

    public static class ConfigExtensions
    {
        public static switch_event_header GetHeader(this switch_event e, String HeaderName)
        {
            for (var x = e.headers; x != null; x = x.next)
            {
                if (HeaderName.ToLower() == x.name.ToLower())
                    return x;
            }
            return null;
        }

        public static String GetValueOfHeader(this switch_event e, String headerName, String defaultValue = "")
        {
            var head = e.GetHeader(headerName);
            return (head == null ? defaultValue : head.value);
        }

        public static Boolean GetBooleanHeader(this switch_event e, String headerName)
        {
            var hv = e.GetValueOfHeader(headerName);
            hv = (String.IsNullOrEmpty(hv) ? "false" : hv);
            Boolean rv = false;
            Boolean.TryParse(hv, out rv);
            return rv;
        }

        public static Guid GetUUID(this Event e)
        {
            var idfld = e.GetHeader("Unique-ID");
            return String.IsNullOrEmpty(idfld) ? Guid.Empty : new Guid(idfld);
        }

        public static void ForEach(this switch_event e, Action<String, String> action)
        {
            for (var x = e.headers; x != null; x = x.next)
            {
                action(x.name, x.value);
            }
        }

        public static Guid GetGuid(this switch_event e)
        {
            return e.GetHeader("Unique-ID") == null ? Guid.Empty : Guid.Parse(e.GetHeader("Unique-ID").value);
        }

        public static void Dump(this switch_event e)
        {
            e.ForEach((n, v) => Console.WriteLine("{0}={1}", n, v));
        }

        public static Dictionary<String, String> ExtractVars(this switch_event e)
        {
            var r = new Dictionary<String, String>();
            for (var x = e.headers; x != null; x = x.next)
            {
                if (x.name.StartsWith("variable_"))
                {
                    r.Add(x.name.ToLower().Replace("variable_", String.Empty),
                          x.value);
                }
            }
            return r;
        }

        public static String ModDigits(this String Orig, int qdel, String Prefix)
        {
            var rv = Orig;
            rv = qdel > Orig.Length ? String.Empty : rv.Substring(qdel);
            if (!String.IsNullOrEmpty(Prefix))
                rv = Prefix + rv;
            return rv;
        }

        //public static String RoutingReplace(this String Orig, String Orignal, String Modified)
        //{
        //    var rv = Orig;
        //    rv = rv.Replace("%d", Modified);
        //    rv = rv.Replace("%o", Orignal);
        //    return rv;
        //}

        public static void MergeFrom<TKey, TValue>(
            this Dictionary<TKey, TValue> dict,
            Dictionary<TKey, TValue> src
            )
        {
            foreach (var entry in src)
            {
                if (!dict.ContainsKey(entry.Key))
                    dict.Add(entry.Key, entry.Value);
            }
        }

        public static void RemoveKeysIn<TKey, TValue>(
            this Dictionary<TKey, TValue> dict,
            IEnumerable<TKey> keys
            )
        {
            foreach (var k in keys)
                if (dict.ContainsKey(k))
                    dict.Remove(k);
        }

        public static void AddVariableTextList(this Dictionary<String, String> d, String varslist)
        {
            if (d == null || String.IsNullOrEmpty(varslist))
                return;
            foreach (var vardef in varslist.Split(",".ToCharArray()))
            {
                var args = vardef.Split("=".ToCharArray(), 2);
                if (!d.ContainsKey(args[0]))
                {
                    d.Add(args[0], args[1]);
                }
                else
                {
                    d[args[0]] = args[1];
                }
            }
        }

        public static String ToParamString(this Dictionary<String, String> d)
        {
            var firstPart = new List<String>();
            foreach (var e in d)
            {
                firstPart.Add(String.Format("{0}={1}", e.Key, e.Value));
            }
            var rv = String.Join(",", firstPart.ToArray());
            return rv;
        }

        public static String ToChannelVars(this Dictionary<String, String> d, Boolean bLocal)
        {
            if (d == null || d.Count == 0)
                return String.Empty;
            String fmt;
            if (bLocal)
                fmt = "[{0}]";
            else
                fmt = "{{{0}}}";
            return String.Format(fmt, d.ToParamString());
        }

        public static void AddActions(this List<String> l, params String[] strings)
        {
            String seperator = "^";
            foreach (var s in strings)
            {
                if (String.IsNullOrEmpty(s))
                    continue;
                l.AddRange(s.Split(seperator.ToCharArray()));
            }
        }
    }

}
