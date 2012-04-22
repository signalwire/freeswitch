-- MySQL dump 10.10
--
-- Host: localhost    Database: easyroute
-- ------------------------------------------------------
-- Server version	5.0.26-log

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
-- Table structure for table `gateways`
--

DROP TABLE IF EXISTS `gateways`;
CREATE TABLE `gateways` (
  `gateway_id` int(10) unsigned NOT NULL auto_increment,
  `gateway_ip` varchar(16) NOT NULL,
  `group` varchar(15) NOT NULL,
  `limit` int(10) unsigned NOT NULL,
  `techprofile` varchar(128) NOT NULL,
  PRIMARY KEY  (`gateway_id`),
  KEY `gateway_ip` (`gateway_ip`,`group`)
) ENGINE=MyISAM AUTO_INCREMENT=2 DEFAULT CHARSET=utf8 COMMENT='Gateways Table';

--
-- Dumping data for table `gateways`
--

LOCK TABLES `gateways` WRITE;
/*!40000 ALTER TABLE `gateways` DISABLE KEYS */;
INSERT INTO `gateways` VALUES (1,'192.168.99.1','mustang',50,'sofia/default');
/*!40000 ALTER TABLE `gateways` ENABLE KEYS */;
UNLOCK TABLES;

--
-- Table structure for table `numbers`
--

DROP TABLE IF EXISTS `numbers`;
CREATE TABLE `numbers` (
  `number_id` int(10) unsigned NOT NULL auto_increment,
  `gateway_id` int(10) unsigned NOT NULL,
  `number` varchar(16) NOT NULL,
  `acctcode` varchar(16) NOT NULL,
  `translated` varchar(16) NOT NULL,
  PRIMARY KEY  (`number_id`),
  UNIQUE KEY `number` (`number`),
  KEY `gateway_id` (`gateway_id`)
) ENGINE=InnoDB DEFAULT CHARSET=utf8 COMMENT='Numbers Table';

--
-- Dumping data for table `numbers`
--

LOCK TABLES `numbers` WRITE;
/*!40000 ALTER TABLE `numbers` DISABLE KEYS */;
INSERT INTO `numbers` VALUES (1,1,'19018577141','999999', '9018577141'),(2,1,'19995551212','666666', '9995551212');
/*!40000 ALTER TABLE `numbers` ENABLE KEYS */;
UNLOCK TABLES;
/*!40103 SET TIME_ZONE=@OLD_TIME_ZONE */;

/*!40101 SET SQL_MODE=@OLD_SQL_MODE */;
/*!40014 SET FOREIGN_KEY_CHECKS=@OLD_FOREIGN_KEY_CHECKS */;
/*!40014 SET UNIQUE_CHECKS=@OLD_UNIQUE_CHECKS */;
/*!40101 SET CHARACTER_SET_CLIENT=@OLD_CHARACTER_SET_CLIENT */;
/*!40101 SET CHARACTER_SET_RESULTS=@OLD_CHARACTER_SET_RESULTS */;
/*!40101 SET COLLATION_CONNECTION=@OLD_COLLATION_CONNECTION */;
/*!40111 SET SQL_NOTES=@OLD_SQL_NOTES */;

-- Dump completed on 2008-02-28 21:41:43
