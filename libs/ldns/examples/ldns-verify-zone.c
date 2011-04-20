/*
 * read a zone file from disk and prints it, one RR per line
 *
 * (c) NLnetLabs 2008
 *
 * See the file LICENSE for the license
 *
 * Missing from the checks: empty non-terminals
 */

#include "config.h"
#include <unistd.h>
#include <stdlib.h>

#include <ldns/ldns.h>

#include <errno.h>

#ifdef HAVE_SSL
#include <openssl/err.h>

int verbosity = 3;

/* returns 1 if the list is empty, or if there are only ns rrs in the
 * list, 0 otherwise */
static int
only_ns_in_rrsets(ldns_dnssec_rrsets *rrsets) {
	ldns_dnssec_rrsets *cur_rrset = rrsets;

	while (cur_rrset) {
		if (cur_rrset->type != LDNS_RR_TYPE_NS) {
			return 0;
		}
		cur_rrset = cur_rrset->next;
	}
	return 1;
}

static int
zone_is_nsec3_optout(ldns_rbtree_t *zone_nodes)
{
	/* simply find the first NSEC3 RR and check its flags */
	/* TODO: maybe create a general function that uses the active
	 * NSEC3PARAM RR? */
	ldns_rbnode_t *cur_node;
	ldns_dnssec_name *cur_name;
	cur_node = ldns_rbtree_first(zone_nodes);
	while (cur_node != LDNS_RBTREE_NULL) {
		cur_name = (ldns_dnssec_name *) cur_node->data;
		if (cur_name && cur_name->nsec &&
		    ldns_rr_get_type(cur_name->nsec) == LDNS_RR_TYPE_NSEC3) {
			if (ldns_nsec3_optout(cur_name->nsec)) {
				return 1;
			} else {
				return 0;
			}
		}
		cur_node = ldns_rbtree_next(cur_node);
	}
	return 0;
}

static bool
ldns_rr_list_contains_name(const ldns_rr_list *rr_list,
					  const ldns_rdf *name)
{
	size_t i;
	for (i = 0; i < ldns_rr_list_rr_count(rr_list); i++) {
		if (ldns_dname_compare(name,
		    ldns_rr_owner(ldns_rr_list_rr(rr_list, i))) == 0) {
			return true;
		}
	}
	return false;
}

static void
print_type(ldns_rr_type type)
{
	const ldns_rr_descriptor *descriptor;

	descriptor = ldns_rr_descript(type);
	if (descriptor && descriptor->_name) {
		fprintf(stdout, "%s", descriptor->_name);
	} else {
		fprintf(stdout, "TYPE%u",
			   type);
	}

}

static ldns_dnssec_zone *
create_dnssec_zone(ldns_zone *orig_zone)
{
	size_t i;
	ldns_dnssec_zone *dnssec_zone;
	ldns_rr *cur_rr;
	ldns_status status;

	/* when reading NSEC3s, there is a chance that we encounter nsecs
	   for empty nonterminals, whose nonterminals we cannot derive yet
	   because the needed information is to be read later. in that case
	   we keep a list of those nsec3's and retry to add them later */
	ldns_rr_list *failed_nsec3s = ldns_rr_list_new();

	dnssec_zone = ldns_dnssec_zone_new();
    	if (ldns_dnssec_zone_add_rr(dnssec_zone, ldns_zone_soa(orig_zone)) !=
	    LDNS_STATUS_OK) {
		if (verbosity > 0) {
			fprintf(stderr,
				   "Error adding SOA to dnssec zone, skipping record\n");
		}
	}

	for (i = 0; i < ldns_rr_list_rr_count(ldns_zone_rrs(orig_zone)); i++) {
		cur_rr = ldns_rr_list_rr(ldns_zone_rrs(orig_zone), i);
		status = ldns_dnssec_zone_add_rr(dnssec_zone, cur_rr);
		if (status != LDNS_STATUS_OK) {
			if (status == LDNS_STATUS_DNSSEC_NSEC3_ORIGINAL_NOT_FOUND) {
				ldns_rr_list_push_rr(failed_nsec3s, cur_rr);
			} else {
				if (verbosity > 0) {
					fprintf(stderr, "Error adding RR to dnssec zone");
					fprintf(stderr, ", skipping record:\n");
					ldns_rr_print(stderr, cur_rr);
				}
			}
		}
	}

	if (ldns_rr_list_rr_count(failed_nsec3s) > 0) {
		(void) ldns_dnssec_zone_add_empty_nonterminals(dnssec_zone);
		for (i = 0; i < ldns_rr_list_rr_count(failed_nsec3s); i++) {
			cur_rr = ldns_rr_list_rr(failed_nsec3s, i);
			status = ldns_dnssec_zone_add_rr(dnssec_zone, cur_rr);
		}
	}

	ldns_rr_list_free(failed_nsec3s);
	return dnssec_zone;
}

static ldns_status
verify_dnssec_rrset(ldns_rdf *zone_name,
					ldns_rdf *name,
                    ldns_dnssec_rrsets *rrset,
                    ldns_rr_list *keys,
                    ldns_rr_list *glue_rrs)
{
	ldns_rr_list *rrset_rrs;
	ldns_dnssec_rrs *cur_rr, *cur_sig;
	ldns_status status;
	ldns_rr_list *good_keys;
	ldns_status result = LDNS_STATUS_OK;

	if (!rrset->rrs) return LDNS_STATUS_OK;

	rrset_rrs = ldns_rr_list_new();
	cur_rr = rrset->rrs;
	while(cur_rr && cur_rr->rr) {
		ldns_rr_list_push_rr(rrset_rrs, cur_rr->rr);
		cur_rr = cur_rr->next;
	}
	cur_sig = rrset->signatures;
	if (cur_sig) {
		while (cur_sig) {
			good_keys = ldns_rr_list_new();
			status = ldns_verify_rrsig_keylist(rrset_rrs,
										cur_sig->rr,
										keys,
										good_keys);
			if (status != LDNS_STATUS_OK) {
				if (verbosity > 0) {
					printf("Error: %s",
						  ldns_get_errorstr_by_id(status));
					printf(" for ");
					ldns_rdf_print(stdout,
								ldns_rr_owner(rrset->rrs->rr));
					printf("\t");
					print_type(rrset->type);
					printf("\n");
					if (result == LDNS_STATUS_OK) {
						result = status;
					}
					if (status == LDNS_STATUS_SSL_ERR) {
						ERR_load_crypto_strings();
						ERR_print_errors_fp(stdout);
					}
					if (verbosity >= 4) {
						printf("RRSet:\n");
						ldns_dnssec_rrs_print(stdout, rrset->rrs);
						printf("Signature:\n");
						ldns_rr_print(stdout, cur_sig->rr);
						printf("\n");
					}
				}
			}
			ldns_rr_list_free(good_keys);

			cur_sig = cur_sig->next;
		}
	} else {
		/* delegations are unsigned */
		if (rrset->type != LDNS_RR_TYPE_NS ||
			ldns_dname_compare(name, zone_name) == 0) {
			if (verbosity > 0) {
				printf("Error: no signatures for ");
				ldns_rdf_print(stdout, ldns_rr_owner(rrset->rrs->rr));
				printf("\t");
				print_type(rrset->type);
				printf("\n");
			}
		}
	}
	ldns_rr_list_free(rrset_rrs);
	return result;
}

static ldns_status
verify_single_rr(ldns_rr *rr,
			  ldns_dnssec_rrs *signature_rrs,
			  ldns_rr_list *keys)
{
	ldns_rr_list *rrset_rrs;
	ldns_rr_list *good_keys;
	ldns_dnssec_rrs *cur_sig;
	ldns_status status;
	ldns_status result = LDNS_STATUS_OK;

	rrset_rrs = ldns_rr_list_new();
	ldns_rr_list_push_rr(rrset_rrs, rr);

	cur_sig = signature_rrs;
	while (cur_sig) {
		good_keys = ldns_rr_list_new();
		status = ldns_verify_rrsig_keylist(rrset_rrs,
									cur_sig->rr,
									keys,
									good_keys);
		if (status != LDNS_STATUS_OK) {
			if (verbosity >= 1) {
				printf("Error: %s ", ldns_get_errorstr_by_id(status));
				if (result == LDNS_STATUS_OK) {
					result = status;
				}
				printf("for ");
				ldns_rdf_print(stdout, ldns_rr_owner(rr));
				printf("\t");
				print_type(ldns_rr_get_type(rr));
				printf("\n");
				if (status == LDNS_STATUS_SSL_ERR) {
					ERR_load_crypto_strings();
					ERR_print_errors_fp(stdout);
				}
				if (verbosity >= 4) {
					printf("RRSet:\n");
					ldns_rr_list_print(stdout, rrset_rrs);
					printf("Signature:\n");
					ldns_rr_print(stdout, cur_sig->rr);
					printf("\n");
				}
			}
			result = status;
		}
		ldns_rr_list_free(good_keys);
		cur_sig = cur_sig->next;
	}

	ldns_rr_list_free(rrset_rrs);

	return result;
}

static ldns_status
verify_next_hashed_name(ldns_rbtree_t *zone_nodes,
                        ldns_dnssec_name *name)
{
	ldns_rbnode_t *next_node;
	ldns_dnssec_name *next_name;
	ldns_dnssec_name *cur_next_name = NULL;
	ldns_dnssec_name *cur_first_name = NULL;
	int cmp;
	char *next_owner_str;
	ldns_rdf *next_owner_dname;

	if (!name->hashed_name) {
		name->hashed_name = ldns_nsec3_hash_name_frm_nsec3(name->nsec,
		                                                   name->name);
	}
	next_node = ldns_rbtree_first(zone_nodes);
	while (next_node != LDNS_RBTREE_NULL) {
		next_name = (ldns_dnssec_name *)next_node->data;
		/* skip over names that have no NSEC3 records (whether it
		 * actually should or should not should have been checked
		 * already */
		if (!next_name->nsec) {
			next_node = ldns_rbtree_next(next_node);
			continue;
		}
		if (!next_name->hashed_name) {
			next_name->hashed_name = ldns_nsec3_hash_name_frm_nsec3(
			                              name->nsec, next_name->name);
		}
		/* we keep track of what 'so far' is the next hashed name;
		 * it must of course be 'larger' than the current name
		 * if we find one that is larger, but smaller than what we
		 * previously thought was the next one, that one is the next
		 */
		cmp = ldns_dname_compare(name->hashed_name,
		                         next_name->hashed_name);
		if (cmp < 0) {
			if (!cur_next_name) {
				cur_next_name = next_name;
			} else {
				cmp = ldns_dname_compare(next_name->hashed_name,
				                         cur_next_name->hashed_name);
				if (cmp < 0) {
					cur_next_name = next_name;
				}
			}
		}
		/* in case the hashed name of the nsec we are checking is the
		 * last one, we need the first hashed name of the zone */
		if (!cur_first_name) {
			cur_first_name = next_name;
		} else {
			cmp = ldns_dname_compare(next_name->hashed_name,
									 cur_first_name->hashed_name);
			if (cmp < 0) {
				cur_first_name = next_name;
			}
		}
		next_node = ldns_rbtree_next(next_node);
	}
	if (!cur_next_name) {
		cur_next_name = cur_first_name;
	}

	next_owner_str = ldns_rdf2str(ldns_nsec3_next_owner(name->nsec));
	next_owner_dname = ldns_dname_new_frm_str(next_owner_str);
	cmp = ldns_dname_compare(next_owner_dname,
	                         cur_next_name->hashed_name);
	ldns_rdf_deep_free(next_owner_dname);
	LDNS_FREE(next_owner_str);
	if (cmp != 0) {
		printf("Error: The NSEC3 record for ");
		ldns_rdf_print(stdout, name->name);
		printf(" points to the wrong next hashed owner name\n");
		printf("(should point to ");
		ldns_rdf_print(stdout, cur_next_name->name);
		printf("(whose hashed name is ");
		ldns_rdf_print(stdout, cur_next_name->hashed_name);
		printf(")\n");
		return LDNS_STATUS_ERR;
	} else {
		return LDNS_STATUS_OK;
	}
}

static ldns_rbnode_t *
next_nonglue_node(ldns_rbnode_t *node, ldns_rr_list *glue_rrs)
{
	ldns_rbnode_t *cur_node = ldns_rbtree_next(node);
	ldns_dnssec_name *cur_name;
	while (cur_node != LDNS_RBTREE_NULL) {
		cur_name = (ldns_dnssec_name *) cur_node->data;
		if (cur_name && cur_name->name) {
			if (!ldns_rr_list_contains_name(glue_rrs, cur_name->name)) {
				return cur_node;
			}
		}
		cur_node = ldns_rbtree_next(cur_node);
	}
	return LDNS_RBTREE_NULL;
}

static ldns_status
verify_nsec(ldns_rbtree_t *zone_nodes,
            ldns_rbnode_t *cur_node,
            ldns_rr_list *keys,
            ldns_rr_list *glue_rrs
)
{
	ldns_rbnode_t *next_node;
	ldns_dnssec_name *name, *next_name;
	ldns_status status, result;
	result = LDNS_STATUS_OK;

	name = (ldns_dnssec_name *) cur_node->data;
	if (name->nsec) {
		if (name->nsec_signatures) {
			status = verify_single_rr(name->nsec,
								 name->nsec_signatures,
								 keys);
			if (result == LDNS_STATUS_OK) {
				result = status;
			}
		} else {
			if (verbosity >= 1) {
				printf("Error: the NSEC(3) record of ");
				ldns_rdf_print(stdout, name->name);
				printf(" has no signatures\n");
			}
			if (result == LDNS_STATUS_OK) {
				result = LDNS_STATUS_ERR;
			}
		}
		/* check whether the NSEC record points to the right name */
		switch (ldns_rr_get_type(name->nsec)) {
			case LDNS_RR_TYPE_NSEC:
				/* simply try next name */
				next_node = next_nonglue_node(cur_node, glue_rrs);
				if (next_node == LDNS_RBTREE_NULL) {
					next_node = ldns_rbtree_first(zone_nodes);
				}
				next_name = (ldns_dnssec_name *)next_node->data;
				if (ldns_dname_compare(next_name->name,
									   ldns_rr_rdf(name->nsec, 0))
					!= 0) {
					printf("Error: the NSEC record for ");
					ldns_rdf_print(stdout, name->name);
					printf(" points to the wrong next owner name\n");
					if (result == LDNS_STATUS_OK) {
						result = LDNS_STATUS_ERR;
					}
				}
				break;
			case LDNS_RR_TYPE_NSEC3:
				/* find the hashed next name in the tree */
				/* this is expensive, do we need to add support
				 * for this in the structs? (ie. pointer to next
				 * hashed name?)
				 */
				status = verify_next_hashed_name(zone_nodes, name);
				if (result == LDNS_STATUS_OK) {
					result = status;
				}
				break;
			default:
				break;
		}
	} else {
		/* todo; do this once and cache result? */
		if (zone_is_nsec3_optout(zone_nodes) &&
		    only_ns_in_rrsets(name->rrsets)) {
			/* ok, no problem, but we need to remember to check
			 * whether the chain does not actually point to this
			 * name later */
		} else {
			if (verbosity >= 1) {
				printf("Error: there is no NSEC(3) for ");
				ldns_rdf_print(stdout, name->name);
				printf("\n");
			}
			if (result == LDNS_STATUS_OK) {
				result = LDNS_STATUS_ERR;
			}
		}
	}
	return result;
}

static int
ldns_dnssec_name_has_only_a(ldns_dnssec_name *cur_name)
{
	ldns_dnssec_rrsets *cur_rrset;
	cur_rrset = cur_name->rrsets;
	while (cur_rrset) {
		if (cur_rrset->type != LDNS_RR_TYPE_A &&
			cur_rrset->type != LDNS_RR_TYPE_AAAA) {
			return 0;
		} else {
			cur_rrset = cur_rrset->next;
		}
	}
	return 1;
}

static ldns_status
verify_dnssec_name(ldns_rdf *zone_name,
                ldns_dnssec_zone *zone,
                ldns_rbtree_t *zone_nodes,
                ldns_rbnode_t *cur_node,
			    ldns_rr_list *keys,
			    ldns_rr_list *glue_rrs)
{
	ldns_status result = LDNS_STATUS_OK;
	ldns_status status;
	ldns_dnssec_rrsets *cur_rrset;
	ldns_dnssec_name *name;
	/* for NSEC chain checks */

	name = (ldns_dnssec_name *) cur_node->data;
	if (verbosity >= 3) {
		printf("Checking: ");
		ldns_rdf_print(stdout, name->name);
		printf("\n");
	}

	if (ldns_rr_list_contains_name(glue_rrs, name->name) &&
		ldns_dnssec_name_has_only_a(name)
	) {
		/* glue */
		cur_rrset = name->rrsets;
		while (cur_rrset) {
			if (cur_rrset->signatures) {
				if (verbosity >= 1) {
					printf("Error: ");
					ldns_rdf_print(stdout, name->name);
					printf("\t");
					print_type(cur_rrset->type);
					printf(" has signature(s), but is glue\n");
				}
				result = LDNS_STATUS_ERR;
			}
			cur_rrset = cur_rrset->next;
		}
		if (name->nsec) {
			if (verbosity >= 1) {
				printf("Error: ");
				ldns_rdf_print(stdout, name->name);
				printf("\thas an NSEC(3), but is glue\n");
			}
			result = LDNS_STATUS_ERR;
		}
	} else {
		/* not glue, do real verify */
		cur_rrset = name->rrsets;
		while(cur_rrset) {
			if (cur_rrset->type != LDNS_RR_TYPE_A ||
			    !ldns_dnssec_zone_find_rrset(zone, name->name, LDNS_RR_TYPE_NS)) {
				status = verify_dnssec_rrset(zone_name, name->name, cur_rrset, keys, glue_rrs);
				if (status != LDNS_STATUS_OK && result == LDNS_STATUS_OK) {
					result = status;
				}
			}
			cur_rrset = cur_rrset->next;
		}

		status = verify_nsec(zone_nodes, cur_node, keys, glue_rrs);
		if (result == LDNS_STATUS_OK) {
			result = status;
		}
	}
	return result;
}

static ldns_status
verify_dnssec_zone(ldns_dnssec_zone *dnssec_zone,
			    ldns_rdf *zone_name,
			    ldns_rr_list *glue_rrs)
{
	ldns_rr_list *keys;
	ldns_rbnode_t *cur_node;
	ldns_dnssec_rrsets *cur_key_rrset;
	ldns_dnssec_rrs *cur_key;
	ldns_dnssec_name *cur_name;
	ldns_status status;
	ldns_status result = LDNS_STATUS_OK;

	keys = ldns_rr_list_new();
	cur_key_rrset = ldns_dnssec_zone_find_rrset(dnssec_zone,
									    zone_name,
									    LDNS_RR_TYPE_DNSKEY);
	if (!cur_key_rrset || !cur_key_rrset->rrs) {
		if (verbosity >= 1) {
			printf("No DNSKEY records at zone apex\n");
		}
		result = LDNS_STATUS_ERR;
	} else {
		cur_key = cur_key_rrset->rrs;
		while (cur_key) {
			if (verbosity >= 4) {
				printf("DNSKEY: ");
				ldns_rr_print(stdout, cur_key->rr);
			}
			ldns_rr_list_push_rr(keys, cur_key->rr);
			cur_key = cur_key->next;
		}

		cur_node = ldns_rbtree_first(dnssec_zone->names);
		if (cur_node == LDNS_RBTREE_NULL) {
			if (verbosity >= 1) {
				printf("Empty zone?\n");
			}
			result = LDNS_STATUS_ERR;
		}
		while (cur_node != LDNS_RBTREE_NULL) {
			cur_name = (ldns_dnssec_name *) cur_node->data;
			status = verify_dnssec_name(zone_name,
			                            dnssec_zone,
			                            dnssec_zone->names,
			                            cur_node,
			                            keys,
			                            glue_rrs);
			if (status != LDNS_STATUS_OK && result == LDNS_STATUS_OK) {
				result = status;
			}
			cur_node = ldns_rbtree_next(cur_node);
		}
	}

	ldns_rr_list_free(keys);
	return result;
}

int
main(int argc, char **argv)
{
	char *filename;
	FILE *fp;
	ldns_zone *z;
	int line_nr = 0;
	int c;
	ldns_status s;
	ldns_dnssec_zone *dnssec_zone;
	ldns_status result = LDNS_STATUS_ERR;
	ldns_rr_list *glue_rrs;

	while ((c = getopt(argc, argv, "hvV:")) != -1) {
		switch(c) {
		case 'h':
			printf("Usage: %s [OPTIONS] <zonefile>\n", argv[0]);
			printf("\tReads the zonefile and checks for DNSSEC errors.\n");
			printf("\nIt checks whether NSEC(3)s are present,");
			printf(" and verifies all signatures\n");
			printf("It also checks the NSEC(3) chain, but it will error on opted-out delegations\n");
			printf("\nOPTIONS:\n");
			printf("\t-h show this text\n");
			printf("\t-v shows the version and exits\n");
			printf("\t-V [0-5]\tset verbosity level (default 3)\n");
			printf("\nif no file is given standard input is read\n");
			exit(EXIT_SUCCESS);
			break;
		case 'v':
			printf("read zone version %s (ldns version %s)\n",
				  LDNS_VERSION, ldns_version());
			exit(EXIT_SUCCESS);
			break;
		case 'V':
			verbosity = atoi(optarg);
			break;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc == 0) {
		fp = stdin;
	} else {
		filename = argv[0];

		fp = fopen(filename, "r");
		if (!fp) {
			fprintf(stderr,
				   "Unable to open %s: %s\n",
				   filename,
				   strerror(errno));
			exit(EXIT_FAILURE);
		}
	}

	s = ldns_zone_new_frm_fp_l(&z, fp, NULL, 0, LDNS_RR_CLASS_IN, &line_nr);

	if (s == LDNS_STATUS_OK) {
		if (!ldns_zone_soa(z)) {
			fprintf(stderr, "; Error: no SOA in the zone\n");
			exit(1);
		}

		glue_rrs = ldns_zone_glue_rr_list(z);
		dnssec_zone = create_dnssec_zone(z);

		if (verbosity >= 5) {
			ldns_dnssec_zone_print(stdout, dnssec_zone);
		}

		result = verify_dnssec_zone(dnssec_zone,
							   ldns_rr_owner(ldns_zone_soa(z)),
							   glue_rrs);


		if (result == LDNS_STATUS_OK) {
			if (verbosity >= 1) {
				printf("Zone is verified and complete\n");
			}
		} else {
			if (verbosity >= 1) {
				printf("There were errors in the zone\n");
			}
		}

		ldns_zone_free(z);
		ldns_dnssec_zone_deep_free(dnssec_zone);
	} else {
		fprintf(stderr, "%s at %d\n",
				ldns_get_errorstr_by_id(s),
				line_nr);
                exit(EXIT_FAILURE);
	}
	fclose(fp);

	exit(result);
}
#else
int
main(int argc, char **argv)
{
	fprintf(stderr, "ldns-verifyzone needs OpenSSL support, which has not been compiled in\n");
	return 1;
}
#endif /* HAVE_SSL */
