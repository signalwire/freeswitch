-- MySQL dump 10.11
--
-- Host: lmdt.intralanman.com    Database: freeswitch
-- ------------------------------------------------------
-- Server version	5.0.51a-17-log

/*!40101 SET @OLD_CHARACTER_SET_CLIENT=@@CHARACTER_SET_CLIENT */;
/*!40101 SET @OLD_CHARACTER_SET_RESULTS=@@CHARACTER_SET_RESULTS */;
/*!40101 SET @OLD_COLLATION_CONNECTION=@@COLLATION_CONNECTION */;
/*!40101 SET NAMES utf8 */;
/*!40103 SET @OLD_TIME_ZONE=@@TIME_ZONE */;
/*!40103 SET TIME_ZONE='+00:00' */;
/*!40014 SET @OLD_UNIQUE_CHECKS=@@UNIQUE_CHECKS, UNIQUE_CHECKS=0 */;
/*!40014 SET @OLD_FOREIGN_KEY_CHECKS=@@FOREIGN_KEY_CHECKS, FOREIGN_KEY_CHECKS=0 */;
/*!40101 SET @OLD_SQL_MODE=@@SQL_MODE, SQL_MODE='NO_AUTO_VALUE_ON_ZERO' */;
/*!40111 SET @OLD_SQL_NOTES=@@SQL_NOTES, SQL_NOTES=0 */;

--
-- Table structure for table `lcr`
--

DROP TABLE IF EXISTS `lcr`;
CREATE TABLE `lcr` (
  `id` int(11) NOT NULL auto_increment,
  `digits` varchar(15) default NULL,
  `rate` float(11,5) unsigned,
  `intrastate_rate` float(11, 5) unsigned,
  `intralata_rate` float(11, 5) unsigned,
  `carrier_id` int(11) NOT NULL,
  `lead_strip` int(11) NOT NULL,
  `trail_strip` int(11) NOT NULL,
  `prefix` varchar(16) NOT NULL,
  `suffix` varchar(16) NOT NULL,
  `lcr_profile` int(11) NOT NULL default 0,
  `date_start` datetime NOT NULL DEFAULT '1970-01-01',
  `date_end` datetime NOT NULL DEFAULT '2030-12-31',
  `quality` float(10,6) NOT NULL,
  `reliability` float(10,6) NOT NULL,
  `cid` varchar(32) NOT NULL DEFAULT '',
  `enabled` boolean NOT NULL DEFAULT '1',
  `lrn` boolean NOT NULL DEFAULT false,
  PRIMARY KEY  (`id`),
  KEY `carrier_id` (`carrier_id`),
  KEY `digits` (`digits`),
  KEY `lcr_profile` (`lcr_profile`),
  KEY `rate` (`rate`),
  KEY `digits_profile_cid_rate` USING BTREE (`digits`,`rate`),
  CONSTRAINT `carrier_id` FOREIGN KEY (`carrier_id`) REFERENCES `carriers` (`id`) ON DELETE CASCADE ON UPDATE CASCADE
) ENGINE=InnoDB AUTO_INCREMENT=8 DEFAULT CHARSET=latin1;

--
-- Table structure for table `carriers`
--

DROP TABLE IF EXISTS `carriers`;
CREATE TABLE `carriers` (
  `id` int(11) NOT NULL auto_increment,
  `carrier_name` varchar(255) default NULL,
  `enabled` boolean NOT NULL DEFAULT '1',
  PRIMARY KEY  (`id`)
) ENGINE=InnoDB AUTO_INCREMENT=13 DEFAULT CHARSET=latin1;

--
-- Table structure for table `carrier_gateway`
--

DROP TABLE IF EXISTS `carrier_gateway`;
CREATE TABLE `carrier_gateway` (
  `id` int(11) NOT NULL auto_increment,
  `carrier_id` int(11) default NULL,
  `prefix` varchar(255) NOT NULL,
  `suffix` varchar(255) NOT NULL,
  `codec` varchar(255) NOT NULL,
  `enabled` boolean NOT NULL DEFAULT '1',
  PRIMARY KEY  (`id`),
  KEY `carrier_id` (`carrier_id`)
) ENGINE=InnoDB AUTO_INCREMENT=17 DEFAULT CHARSET=latin1;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2008-11-21 22:48:11
