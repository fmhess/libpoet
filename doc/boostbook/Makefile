PROJECT_ROOT="../../.."
BOOST_SRC_DIR=~/svn/boost_docs/
BOOSTBOOK_TOOL_DIR=$(BOOST_SRC_DIR)/tools/boostbook

XML_SRCS=\
	active_function_hpp.xml \
	active_object_hpp.xml \
	acyclic_mutex_base_hpp.xml \
	acyclic_mutex_hpp.xml \
	exception_ptr_hpp.xml \
	exceptions_hpp.xml \
	future_hpp.xml \
	future_barrier_hpp.xml \
	future_select_hpp.xml \
	libpoet_doc.xml \
	libpoet_reference.xml \
	monitor_base_hpp.xml \
	monitor_ptr_hpp.xml \
	monitor_hpp.xml \
	monitor_locks_hpp.xml \
	mutex_grapher_hpp.xml \
	mutex_properties_hpp.xml

.PHONY: all
all: doc

.PHONY: clean
clean:
	$(RM) -r html/*
	$(RM) libpoet_doc.docbook

.PHONY: doc
doc: html

.PHONY: html
html: libpoet_doc.docbook
	(export XML_CATALOG_FILES="./catalog.xml /etc/xml/catalog";\
		xsltproc --xinclude --stringparam boost.root $(PROJECT_ROOT) \
		--stringparam chapters.select.box.show false \
		--stringparam google.search.box.show false \
		--stringparam toc.max.depth 4 \
		--stringparam toc.section.depth 3 \
		-o html/ $(BOOSTBOOK_TOOL_DIR)/xsl/html.xsl $<)
	cp ./demo_locking_order_graph.png html/

libpoet_doc.docbook: $(XML_SRCS)
	(export XML_CATALOG_FILES="./catalog.xml /etc/xml/catalog";\
		xsltproc --xinclude --stringparam boost.root $(PROJECT_ROOT) -o $@ $(BOOSTBOOK_TOOL_DIR)/xsl/docbook.xsl libpoet_doc.xml)
