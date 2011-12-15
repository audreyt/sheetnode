SHEETNODE_RDF
This submodule of the sheetnode module allows you to export data from spreadsheet cells as 

RDF triples that can be queried from the SPARQL endpoint of the website. 


REQUIREMENTS
This module also requires that a patch be made to the RDFX module, as described here:
http://drupal.org/node/1240778


RDF MAPPING FILE
The RDF output requires an XML mapping file to designate what will be exported from
the 
spreadsheet cells. 
The format for the RDF mapping file is an XML file that is 
based on a triple 
format. An example follows.



<?xml version="1.0" ?>

<spos>

<subject>
  
	<uri>D4</uri>
  
	<predicate>
    
		<vocab>dc:title</vocab>
    
		<object>
      
			<literal>E4</literal>
    
		</object>
  
	</predicate>

</subject>

</spos>





In this example, the uri of cell D4 is the subject, the dc:title is the 
vocabulary (dc) and 
its property (title) for the predicate. Note that for 
the vocabulary to be properly 
de-referenced to a URI, then the vocabulary 
must have been loaded into the Drupal instance.  


The object in this case will become a literal value and uses the value set
in cell E4. 


So the resulting triple would look something like this. 



<http://example.com/node/1#D4>

<http://purl.org/dc/elements#title>

Title for My Sheetnode Cell



Where in this case, the sheetnode is at node/1 and the contents of cell E4
in the spreadsheet is 
"Title for My Sheetnode Cell". 

Alternatively, the <object> tags can use the following child tags 
instead 
of the <literal> tag. 



<uri>

<literal_multi>



The <uri> object tag will create a <uri> in the RDF output rather than a 
literal. For example, 
in the above example if we used the <uri> instead of the
<literal> for the object tag, then we 
would have the following triple. 



<http://example.com/node/1#D4>

<http://purl.org/dc/elements#title>

<http://example.com/node/1#E4>



For the <literal_multi> object tag, we assume that there are multiple literal 
values in the 
given cell that are separated by commas. For example, if cell 
E4 in the above example had the 
contents of "Title 1, Title 2" and the mapping 
file used <literal_multi> instead of <literal>, 
then we would have the following 
2 RDF triples produced. 



<http://example.com/node/1#D4>

<http://purl.org/dc/elements#title>

Title 1



<http://example.com/node/1#D4>

<http://purl.org/dc/elements#title>

Title 2



In this case, it may not make much sense since title is usually a one-to-one 
mapping, but for 
other vocabulary properties where a one-to-many mapping is 
appropriate, then this option 
becomes useful. 



Note that this mapping approach does not cover all use-cases and that we expect 
this approach 
to evolve as more use-cases need to be covered. 



