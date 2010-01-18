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
      eslConnection.sendRecv("event plain ALL");


      // Grab Events until process is killed
      while (eslConnection.connected() == 1)
      {
        ESLevent eslEvent = eslConnection.recvEvent();
        Console.WriteLine(eslEvent.serialize(String.Empty));
      }
    }
  }
}
