DROP TABLE gallery_entry;
DROP SEQUENCE gallery_entry_seq;
CREATE SEQUENCE gallery_entry_seq;
CREATE TABLE gallery_entry (
  id INTEGER PRIMARY KEY DEFAULT NEXTVAL('gallery_entry_seq'),
  author VARCHAR,
  created_at VARCHAR DEFAULT 'plop', --TIMESTAMP
  path VARCHAR,
  alias VARCHAR,
  secured VARCHAR,
  description VARCHAR
);

DROP TABLE gallery_users;
DROP SEQUENCE gallery_users_seq;
CREATE SEQUENCE gallery_users_seq;
CREATE TABLE gallery_users (
  id INTEGER PRIMARY KEY DEFAULT NEXTVAL('gallery_users_seq'),
  name VARCHAR,
  password VARCHAR,
  isadmin VARCHAR
);
DROP TABLE gallery_stats;
DROP SEQUENCE gallery_stats_seq;
CREATE SEQUENCE gallery_stats_seq;
CREATE TABLE gallery_stats (
  id INTEGER PRIMARY KEY DEFAULT NEXTVAL('gallery_stats_seq'),
  path VARCHAR,
  preview INTEGER,
  download INTEGER
);

DROP TABLE gallery_vdir;
DROP SEQUENCE gallery_vdir_seq;
CREATE SEQUENCE gallery_vdir_seq;
CREATE TABLE gallery_vdir (
  id INTEGER PRIMARY KEY DEFAULT NEXTVAL('gallery_vdir_seq'),
  owner VARCHAR,
  path VARCHAR,
  name VARCHAR,
  date INTEGER
);

DROP TABLE gallery_vphoto;
CREATE TABLE gallery_vphoto (
  dir VARCHAR,
  name VARCHAR,
  realpath INTEGER
);

DROP TABLE gallery_process_ts;
CREATE TABLE gallery_process_ts (
	filename VARCHAR,
	date INTEGER
);

DROP TABLE gallery_process;
CREATE TABLE gallery_process (
	filename VARCHAR,
	upto INTEGER,

	opid INTEGER,
	opserial INTEGER,
	enabled INTEGER,
	opname VARCHAR,
	param VARCHAR,
	value VARCHAR
);

DROP TABLE gallery_pass;
CREATE TABLE gallery_pass (
	id VARCHAR,
	name VARCHAR,
	path VARCHAR
);

DROP TABLE gallery_white_balance;
CREATE TABLE gallery_white_balance (
    id INTEGER,
    op VARCHAR,
    rx NUMERIC,
    gx NUMERIC,
    bx NUMERIC,
    "desc" VARCHAR,
    dropped INTEGER
);

INSERT INTO gallery_white_balance VALUES ( '-1','','0','0','0','Manual','0');
INSERT INTO gallery_white_balance VALUES ( '0','','0','0','0','Without','0');
INSERT INTO gallery_white_balance VALUES ( '1','-a','0','0','0','Automatic','0');
INSERT INTO gallery_white_balance VALUES ( '2','-w','0','0','0','Camera','0');
INSERT INTO gallery_white_balance VALUES ('3','-r','0.554498','1.06973','2.20154','_D80_ Incandescent','1');
INSERT INTO gallery_white_balance VALUES ('4','-r','0.813393','1.06973','1.95252','_D80_ Fluorescent','1');
INSERT INTO gallery_white_balance VALUES ('5','-r','0.866041','1.06973','1.26448','_D80_ Lumière du jour','1');
INSERT INTO gallery_white_balance VALUES ('6','-r','0.969889','1.06973','1.1455','_D80_ Flash','1');
INSERT INTO gallery_white_balance VALUES ('7','-r','0.947187','1.06973','1.15657','_D80_ Nuageux','1');
INSERT INTO gallery_white_balance VALUES ('8','-r','1.09982','1.06973','1.00531','_D80_ Ombre','1');
INSERT INTO gallery_white_balance VALUES ('9','-r','0.754465','1.06973','1.40467','_D80_ 4300K','1');
INSERT INTO gallery_white_balance VALUES ('10','-r','0.84334','1.06973','1.30045','_D80_ 5000K','1');
INSERT INTO gallery_white_balance VALUES ('11','-r','0.937527','1.06973','1.16764','_D80_ 5900K','1');
INSERT INTO gallery_white_balance VALUES ('12','-r','1','1','1','Day light','0');
INSERT INTO gallery_white_balance VALUES ('13','-r','0.565638','1.0726','2.08237','TUNGSTEN','0');
INSERT INTO gallery_white_balance VALUES ('14','-r','0.612932','1.0726','1.84358','TUNGSTEN A6','0');
INSERT INTO gallery_white_balance VALUES ('15','-r','0.605365','1.0726','1.8787','TUNGSTEN A5','0');
INSERT INTO gallery_white_balance VALUES ('16','-r','0.595906','1.0726','1.92084','TUNGSTEN A4','0');
INSERT INTO gallery_white_balance VALUES ('17','-r','0.588339','1.0726','1.95946','TUNGSTEN A3','0');
INSERT INTO gallery_white_balance VALUES ('18','-r','0.580772','1.0726','1.99809','TUNGSTEN A2','0');
INSERT INTO gallery_white_balance VALUES ('19','-r','0.573205','1.0726','2.04023','TUNGSTEN A1','0');
INSERT INTO gallery_white_balance VALUES ('20','-r','0.558071','1.0726','2.12451','TUNGSTEN B1','0');
INSERT INTO gallery_white_balance VALUES ('21','-r','0.550504','1.0726','2.16665','TUNGSTEN B2','0');
INSERT INTO gallery_white_balance VALUES ('22','-r','0.544828','1.0726','2.20879','TUNGSTEN B3','0');
INSERT INTO gallery_white_balance VALUES ('23','-r','0.537261','1.0726','2.25444','TUNGSTEN B4','0');
INSERT INTO gallery_white_balance VALUES ('24','-r','0.531586','1.0726','2.30009','TUNGSTEN B5','0');
INSERT INTO gallery_white_balance VALUES ('25','-r','0.525911','1.0726','2.34223','TUNGSTEN B6','0');
INSERT INTO gallery_white_balance VALUES ('26','-r','0.51456','1.0726','2.21581','Lampes à vapeur de sodium','0');
INSERT INTO gallery_white_balance VALUES ('27','-r','0.565638','1.0726','1.89625','FLUO blanches chaudes','0');
INSERT INTO gallery_white_balance VALUES ('28','-r','0.660226','1.0726','2.18069','FLUO blanches','0');
INSERT INTO gallery_white_balance VALUES ('29','-r','0.817243','1.0726','1.88923','FLUO blanches froides','0');
INSERT INTO gallery_white_balance VALUES ('30','-r','0.828594','1.0726','1.26768','FLUO blanches diurnes','0');
INSERT INTO gallery_white_balance VALUES ('31','-r','0.945883','1.0726','1.0008','FLUO lumière diurne','0');
INSERT INTO gallery_white_balance VALUES ('32','-r','1.10857','1.0726','1.22554','Lampes à vapeur de mercure température élevée','0');
INSERT INTO gallery_white_balance VALUES ('33','-r','0.959125','1.0726','1.07806','Sunny A6','0');
INSERT INTO gallery_white_balance VALUES ('34','-r','0.943991','1.0726','1.09913','Sunny A5','0');
INSERT INTO gallery_white_balance VALUES ('35','-r','0.930749','1.0726','1.1202','Sunny A4','0');
INSERT INTO gallery_white_balance VALUES ('36','-r','0.917506','1.0726','1.14126','Sunny A3','0');
INSERT INTO gallery_white_balance VALUES ('37','-r','0.904264','1.0726','1.16585','Sunny A2','0');
INSERT INTO gallery_white_balance VALUES ('38','-r','0.892913','1.0726','1.19043','Sunny A1','0');
INSERT INTO gallery_white_balance VALUES ('39','-r','0.879671','1.0726','1.21852','Sunny','0');
INSERT INTO gallery_white_balance VALUES ('40','-r','0.868321','1.0726','1.24661','Sunny B1','0');
INSERT INTO gallery_white_balance VALUES ('41','-r','0.855078','1.0726','1.27119','Sunny B2','0');
INSERT INTO gallery_white_balance VALUES ('42','-r','0.841836','1.0726','1.29929','Sunny B3','0');
INSERT INTO gallery_white_balance VALUES ('43','-r','0.828594','1.0726','1.32387','Sunny B4','0');
INSERT INTO gallery_white_balance VALUES ('44','-r','0.815351','1.0726','1.34845','Sunny B5','0');
INSERT INTO gallery_white_balance VALUES ('45','-r','0.800217','1.0726','1.36952','Sunny B6','0');
INSERT INTO gallery_white_balance VALUES ('46','-r','0.987501','1.0726','1.05347','Flash','0');
INSERT INTO gallery_white_balance VALUES ('47','-r','0.943991','1.0726','1.09913','Cloud','0');
INSERT INTO gallery_white_balance VALUES ('48','-r','0.989394','1.0726','1.04294','Cloud A3','0');
INSERT INTO gallery_white_balance VALUES ('49','-r','0.904264','1.0726','1.16585','Cloud B3','0');
INSERT INTO gallery_white_balance VALUES ('50','-r','1.08777','1.0726','0.976221','Shadow','0');
INSERT INTO gallery_white_balance VALUES ('51','-r','1.15398','1.0726','0.930569','Shadow A3','0');
INSERT INTO gallery_white_balance VALUES ('52','-r','1.02912','1.0726','1.01134','Shadow B3','0');


INSERT INTO gallery_users ( id, name,password,isadmin ) VALUES (1, 'admin','zzA/GXehCIrWk','true' );

