-- Table: carriers

-- DROP TABLE carriers;

CREATE TABLE carriers
(
  id serial NOT NULL,
  carrier_name VARCHAR(255) NOT NULL,
  enabled boolean NOT NULL DEFAULT 'true',
  CONSTRAINT carriers_pkey PRIMARY KEY (id)
);

-- Table: carrier_gateway

-- DROP TABLE carrier_gateway;

CREATE TABLE carrier_gateway
(
  id serial NOT NULL,
  carrier_id integer REFERENCES carriers(id),
  prefix VARCHAR(128) NOT NULL DEFAULT '',
  suffix VARCHAR(128) NOT NULL DEFAULT '',
  codec VARCHAR(128) NOT NULL DEFAULT '',
  enabled boolean NOT NULL DEFAULT 'true',
  CONSTRAINT carrier_gateway_pkey PRIMARY KEY (id)
);

-- Index: gateway

-- DROP INDEX gateway;

CREATE UNIQUE INDEX gateway
  ON carrier_gateway
  (prefix, suffix);


-- Table: lcr

-- DROP TABLE lcr;

CREATE TABLE lcr
(
  id serial NOT NULL,
  digits NUMERIC(20, 0),
  rate numeric(11,5) NOT NULL,
  intrastate_rate numeric(11,5) NOT NULL,
  intralata_rate numeric(11,5) NOT NULL,
  carrier_id integer NOT NULL REFERENCES carriers(id),
  lead_strip integer NOT NULL DEFAULT 0,
  trail_strip integer NOT NULL DEFAULT 0,
  prefix VARCHAR(16) NOT NULL DEFAULT '',
  suffix VARCHAR(16) NOT NULL DEFAULT '',
  lcr_profile INTEGER NOT NULL DEFAULT 0,
  date_start timestamp with time zone NOT NULL DEFAULT '1970-01-01',
  date_end timestamp with time zone NOT NULL DEFAULT '2030-12-31',
  quality numeric(10,6) NOT NULL DEFAULT 0,
  reliability numeric(10,6) NOT NULL DEFAULT 0,
  cid VARCHAR(32) NOT NULL DEFAULT '',
  enabled boolean NOT NULL DEFAULT 'true',
  CONSTRAINT lcr_pkey PRIMARY KEY (id)
);

-- Index: digits_rate

-- DROP INDEX digits_rate;

CREATE INDEX digits_rate
  ON lcr
  USING btree
  (digits, rate);

-- Index: profile_digits_15

-- DROP INDEX profile_digits_15;

CREATE INDEX profile_digits_15
  ON lcr
  (digits, lcr_profile);

-- Index: unique_route

-- DROP INDEX unique_route;

CREATE INDEX unique_route
  ON lcr
  (digits, carrier_id);
