<?xml version="1.0" encoding="UTF-8"?>
<xsl:stylesheet xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns:xs="http://www.w3.org/2001/XMLSchema" exclude-result-prefixes="xs" version="2.0">

    <xsl:template match="/">
        <xsl:variable name="master_xml">
            <xsl:copy-of select="document('phrase_en.xml')/*"/>
        </xsl:variable>
        <xsl:variable name="current_xml">
            <xsl:copy-of select="*"/>
        </xsl:variable>
        <xsl:apply-templates select="$master_xml" mode="checkElements">
            <xsl:with-param name="current_xml" select="$current_xml/*"/>
        </xsl:apply-templates>
    </xsl:template>

    <xsl:template match="en" mode="checkElements">
        <xsl:param name="current_xml"/>
        <xsl:copy>
            <xsl:attribute name="translated-to" select="name($current_xml)"/>
            <xsl:apply-templates select="@*|node()" mode="#current">
                <xsl:with-param name="current_xml" select="$current_xml/*"/>
            </xsl:apply-templates>
        </xsl:copy>
    </xsl:template>

    <xsl:template match="prompt" mode="checkElements">
        <xsl:param name="current_xml"/>
        <xsl:variable name="translation"
            select="$current_xml[@filename=current()/@filename]/@phrase"/>
        <xsl:copy>
            <xsl:apply-templates select="@*" mode="#current"/>
            <xsl:choose>
                <xsl:when test="count($translation)=0">
                    <xsl:attribute name="translated" select="false()"/>
                </xsl:when>
                <xsl:when test="@phrase = $translation">
                    <xsl:attribute name="translated" select="false()"/>
                </xsl:when>
                <xsl:otherwise>
                    <xsl:attribute name="translated-to" select="$translation[1]"/>
                </xsl:otherwise>
            </xsl:choose>
            <xsl:if test="count($translation) > 1">
                <xsl:attribute name="more-then-one-translation" select="true()"/>
            </xsl:if>
        </xsl:copy>
    </xsl:template>

    <xsl:template match="@*|node()" mode="checkElements">
        <xsl:param name="current_xml"/>
        <xsl:copy>
            <xsl:apply-templates select="@*|node()" mode="#current">
                <xsl:with-param name="current_xml" select="$current_xml/*"/>
            </xsl:apply-templates>
        </xsl:copy>
    </xsl:template>

</xsl:stylesheet>

