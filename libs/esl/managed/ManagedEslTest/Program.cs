/*
 * C# ESL managed examples 
 * 
 * Version: MPL 1.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 * 
 * Contributor: 
 * Diego Toro <dftoro@yahoo.com>
 */

using System;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace ManagedEslTest
{
  [Flags]
  public enum enLogLevel
  {
    EMERG = 0,
    ALERT,
    CRIT,
    ERROR,
    WARNING,
    NOTICE,
    INFO,
    DEBUG
  }


  class Program
  {
    public static readonly int ESL_SUCCESS = 1;

    static void Main(string[] args)
    {
      /*
       * The next lines are usefull to view simples C# examples about how ESL managed works.  
       * Active only a mode uncomment it: inbound, outbound sync or outbound async. 
       * If you active two o more on this application you may have problems.
       * Remember modify your dialplan for testing outbound modes (see OutboundModeSync and OutboundModeAsync members)
       */
      ThreadPool.QueueUserWorkItem(new WaitCallback(InboundMode));
      //ThreadPool.QueueUserWorkItem(new WaitCallback(OutboundModeSync));
      //ThreadPool.QueueUserWorkItem(new WaitCallback(OutboundModeAsync));

      Console.ReadLine();
    }

    /// <summary>
    /// Example using event socket in "Inbound" mode
    /// </summary>
    /// <param name="stateInfo">Object state info</param>
    static void InboundMode(Object stateInfo)
    {
      //Initializes a new instance of ESLconnection, and connects to the host $host on the port $port, and supplies $password to freeswitch
      ESLconnection eslConnection = new ESLconnection("127.0.0.1", "8021", "ClueCon");

      if (eslConnection.Connected() != ESL_SUCCESS)
      {
        Console.WriteLine("Error connecting to FreeSwitch");
        return;
      }

      //Set log level
      //ESL.eslSetLogLevel((int)enLogLevel.DEBUG);

      // Subscribe to all events 
      ESLevent eslEvent = eslConnection.SendRecv("event plain ALL");

      if (eslEvent == null)
      {
        Console.WriteLine("Error subscribing to all events");
        return;
      }

      //Turns an event into colon-separated 'name: value' pairs. The format parameter isn't used
      Console.WriteLine(eslEvent.Serialize(String.Empty));

      // Grab Events until process is killed
      while (eslConnection.Connected() == ESL_SUCCESS)
      {
        eslEvent = eslConnection.RecvEvent();
        Console.WriteLine(eslEvent.Serialize(String.Empty));
      }
    }

    /// <summary>
    /// Example using event socket in "Outbound" mode synchronic
    /// </summary>
    /// <param name="stateInfo">Object state info</param>
    static void OutboundModeSync(Object stateInfo)
    {
      /* add next line to a dialplan
      <action application="socket" data="localhost:8022 sync full"/> 
      */
      TcpListener tcpListener = new TcpListener(IPAddress.Parse("127.0.0.1"), 8022);

      try
      {
        tcpListener.Start();

        Console.WriteLine("OutboundModeSync, waiting for a connection...");

        while (true)
        {
          Socket sckClient = tcpListener.AcceptSocket();

          //Initializes a new instance of ESLconnection, and connects to the host $host on the port $port, and supplies $password to freeswitch
          ESLconnection eslConnection = new ESLconnection(sckClient.Handle.ToInt32());

          Console.WriteLine("Execute(\"answer\")");
          eslConnection.Execute("answer", String.Empty, String.Empty);
          Console.WriteLine("Execute(\"playback\")");
          eslConnection.Execute("playback", "music/8000/suite-espanola-op-47-leyenda.wav", String.Empty);
          Console.WriteLine("Execute(\"hangup\")");
          eslConnection.Execute("hangup", String.Empty, String.Empty);
        }
      }
      catch (Exception ex)
      {
        Console.WriteLine(ex);
      }
      finally
      {
        tcpListener.Stop();
      }
    }

    /// <summary>
    /// Example using event socket in "Outbound" mode asynchronic
    /// </summary>
    /// <param name="stateInfo">Object state info</param>
    static void OutboundModeAsync(Object stateInfo)
    {
      /* add next line to a dialplan
       <action application="socket" data="localhost:8022 async full" />
      */
      TcpListener tcpListener = new TcpListener(IPAddress.Parse("127.0.0.1"), 8022);

      try
      {
        tcpListener.Start();

        Console.WriteLine("OutboundModeAsync, waiting for connections...");

        while (true)
        {
          tcpListener.BeginAcceptSocket((asyncCallback) =>
         {
           TcpListener tcpListened = (TcpListener)asyncCallback.AsyncState;

           Socket sckClient = tcpListened.EndAcceptSocket(asyncCallback);

           //Initializes a new instance of ESLconnection, and connects to the host $host on the port $port, and supplies $password to freeswitch
           ESLconnection eslConnection = new ESLconnection(sckClient.Handle.ToInt32());

           ESLevent eslEvent = eslConnection.GetInfo();
           string strUuid = eslEvent.GetHeader("UNIQUE-ID", -1);

           eslConnection.SendRecv("myevents");
           eslConnection.SendRecv("divert_events on");

           eslConnection.Execute("answer", String.Empty, String.Empty);
           eslConnection.Execute("playback", "music/8000/suite-espanola-op-47-leyenda.wav", String.Empty);

           while (eslConnection.Connected() == ESL_SUCCESS)
           {
             eslEvent = eslConnection.RecvEvent();
             Console.WriteLine(eslEvent.Serialize(String.Empty));
           }

           sckClient.Close();
           Console.WriteLine("Connection closed uuid:{0}", strUuid);

         }, tcpListener);

          Thread.Sleep(50);
        }
      }
      catch (Exception ex)
      {
        Console.WriteLine(ex);
      }
      finally
      {
        tcpListener.Stop();
      }
    }
  }
}
