package org.freeswitch;

import java.util.*;

public class Event
{
    private String m_body;
    private TreeMap<String,String> m_headers = new TreeMap<String,String>();

    private void setBody(String body)
    {
        m_body = body;
    }

    private void addHeader(String name, String value)
    {
        m_headers.put(name, value);
    }

    public String getBody()
    {
        return m_body;
    }

    public TreeMap<String,String> getHeaders()
    {
        return m_headers;
    }
}

