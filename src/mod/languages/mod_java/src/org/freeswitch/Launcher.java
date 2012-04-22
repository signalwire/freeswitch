/*
 * Launcher.java
 *
 * Created on 13 September 2007, 06:40
 *
 * To change this template, choose Tools | Template Manager
 * and open the template in the editor.
 */

package org.freeswitch;

import java.io.*;
import java.net.*;
import java.lang.reflect.*;

/**
 *
 * @author dacha
 */
public class Launcher
{
    static
    {
        // Find and load mod_java
        String javaLibraryPaths = System.getProperty("java.library.path");
        String pathSeparator = System.getProperty("path.separator");
        String libraryPaths[] = javaLibraryPaths.split(pathSeparator);
        
        String libraryName = System.mapLibraryName("mod_java");
        int modJavaIndex = libraryName.indexOf("mod_java");
        if (modJavaIndex >= 0)
            libraryName = libraryName.substring(modJavaIndex);
        
        for (String libraryPath : libraryPaths)
        {
            String fullLibraryPath = libraryPath + File.separatorChar + libraryName;
            if (new File(fullLibraryPath).exists())
            {
                System.load(fullLibraryPath);
                break;
            }
        }
    }

    public static void launch(String sessionUuid, String args) throws Exception
    {
        String argv[] = args.split("[ ]");
        if (argv.length == 0)
        {
            System.out.println("Too few arguments to mod java");
            System.out.println("Usage: java /path/to/file.jar fully.qualified.class arg1 arg2 arg3");
            System.out.println("Usage: java fully.qualified.class arg1 arg2 arg3");
            return;
        }
        
        Class klazz;
        int argsOffset;
        if (argv[0].endsWith(".jar") || argv[0].endsWith(".JAR"))
        {
            if (argv.length < 2)
                throw new Exception("Too few arguments: must specify fully qualified class name when loading from JAR file");
            URL urls[] = new URL[1];
            urls[0] = new File(argv[0]).toURI().toURL();
            URLClassLoader urlClassLoader = new URLClassLoader(urls);
            klazz = Class.forName(argv[1], true, urlClassLoader);
            argsOffset = argv[0].length() + argv[1].length() + 2;
        }
        else
        {
            klazz = Class.forName(argv[0]);
            argsOffset = argv[0].length() + 1;
        }
        
        Constructor constructor = klazz.getConstructor();
        Object object = constructor.newInstance();
        Method run = klazz.getMethod("run", String.class, String.class);
        String newArgs = "";
        if (argsOffset < args.length())
            newArgs = args.substring(argsOffset);
        run.invoke(object, sessionUuid, newArgs);
    }
}
