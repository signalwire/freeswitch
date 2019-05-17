<?xml version="1.0" encoding="utf-8"?>
<xsl:stylesheet version="1.0"
    xmlns:xsl="http://www.w3.org/1999/XSL/Transform"
    xmlns="http://schemas.microsoft.com/wix/2006/wi"
    xmlns:wix="http://schemas.microsoft.com/wix/2006/wi">

 <!-- <xsl:key name="unusedcomponent-search" match="wix:Component[not (contains(wix:File/@Source, '.dll')) or contains(wix:File/@Source, 'mod')]" use="@Id"/> -->
 <xsl:key name="unusedcomponent-search" match="wix:Component[not (contains(wix:File/@Source, '.dll'))]" use="@Id"/>
 <xsl:key name="unusedcomponent-openh264" match="wix:Component[contains(wix:File/@Source, 'openh264.dll')]" use="@Id"/>

  <!-- strip all extraneous whitespace -->
  <xsl:strip-space  elements="*"/>

  <!-- Copy all attributes and elements to the output. -->
  <xsl:template match="@*|*">
    <xsl:copy>
      <xsl:apply-templates select="@*" />
      <xsl:apply-templates select="*" />
    </xsl:copy>
  </xsl:template>

  <!-- Exclude all File elements that are not a .dll file -->
  <xsl:template match="wix:Component[not(contains(wix:File/@Source, '.dll'))]" />
  <!-- Exclude openh264.dll (See https://www.openh264.org/BINARY_LICENSE.txt) -->
  <xsl:template match="wix:Component[contains(wix:File/@Source, 'openh264.dll')]" />

  <!-- Exclude Directory elements -->
  <!--<xsl:template match="wix:Directory[not(contains(*/@Source, '.pdb'))]"/>-->
  <xsl:template match="wix:Directory[@Name='htdocs']"/>
  <xsl:template match="wix:Directory[@Name='images']"/>
  <xsl:template match="wix:Directory[@Name='fonts']"/>
  <xsl:template match="wix:Directory[@Name='conf']"/>
  <xsl:template match="wix:Directory[@Name='sounds']"/>
  <xsl:template match="wix:Directory[@Name='grammar']"/>

  
  <!-- Remove ComponentRefs. --> 
  <xsl:template match="wix:ComponentRef[key('unusedcomponent-search', @Id)]"/>
  <xsl:template match="wix:ComponentRef[key('unusedcomponent-openh264', @Id)]"/>

</xsl:stylesheet>
