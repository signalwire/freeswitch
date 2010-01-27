using System;

namespace ManagedEslTest
{
  class Program
  {
    static void Main(string[] args)
    {
      // Connect to FreeSWITCH
      ESLconnection eslConnection = new ESLconnection("localhost", "8021", "ClueCon");
      // We want all Events (probably will want to change this depending on your needs)
      eslConnection.SendRecv("event plain ALL");


      // Grab Events until process is killed
      while (eslConnection.Connected() == 1)
      {
        ESLevent eslEvent = eslConnection.RecvEvent();
        Console.WriteLine(eslEvent.Serialize(String.Empty));
      }
    }
  }
}
