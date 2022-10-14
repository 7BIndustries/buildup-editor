/******************************************************************************
 * bue_preprocess-- Converts any BuildUp-specific tags to markdown, including *
 *          searching through and collating data from all files.              *
 *                                                                            *
 * Author: 7B Industries                                                      *
 * License: Apache 2.0                                                        *
 *                                                                            *
 * ***************************************************************************/

/******************************************************************************
 * preprocess -- The primary function that is called that delagates to other  *
 *               functions to handle the different types of BuildUp tags that *
 *               can be used.                                                 *
 *                                                                            *
 * Parameters                                                                 *
 *      buildup_md -- Character pointer holding the markdown with BuildUp     *
 *                    tags embedded within it.                                *
 *                                                                            *
 * Returns                                                                    *
 *      A character pointer to a string with all of the BuildUp tags replaced *
 *      with their collated markdown data.                                    *
 *****************************************************************************/
char* preprocess(char* buildup_md) {
    return buildup_md;
}