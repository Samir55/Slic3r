#include "TMF.hpp"

namespace Slic3r { namespace IO {

bool
TMFEditor::write_types()
{
    // Create a new zip entry "[Content_Types].xml" at zip directory /.
    if(zip_entry_open(zip_archive, "[Content_Types].xml"))
        return false;

    // Write 3MF Types.
    append_buffer("<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n");
    append_buffer("<Types xmlns=\"" + namespaces.at("content_types") + "\">\n");
    append_buffer("<Default Extension=\"rels\" ContentType=\"application/vnd.openxmlformats-package.relationships+xml\"/>\n");
    append_buffer("<Default Extension=\"model\" ContentType=\"application/vnd.ms-package.3dmanufacturing-3dmodel+xml\"/>\n");
    append_buffer("</Types>\n");
    write_buffer();

    // Close [Content_Types].xml zip entry.
    zip_entry_close(zip_archive);

    return true;
}

bool
TMFEditor::write_relationships()
{
    // Create .rels in "_rels" folder in the zip archive.
    if(zip_entry_open(zip_archive, "_rels/.rels"))
        return false;

    // Write the primary 3dmodel relationship.
    append_buffer("<?xml version=\"1.0\" encoding=\"UTF-8\"?> \n"
                          "<Relationships xmlns=\"" + namespaces.at("relationships") +
                  "\">\n<Relationship Id=\"rel0\" Target=\"/3D/3dmodel.model\" Type=\"http://schemas.microsoft.com/3dmanufacturing/2013/01/3dmodel\" /></Relationships>\n");
    write_buffer();

    // Close _rels.rels
    zip_entry_close(zip_archive);
    return true;
}

bool
TMFEditor::write_model()
{
    // Create a 3dmodel.model entry in /3D/ containing the buffer.
    if(zip_entry_open(zip_archive, "3D/3dmodel.model"))
        return false;

    // add the XML document header.
    append_buffer("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    // Write the model element. Append any necessary namespaces.
    append_buffer("<model unit=\"millimeter\" xml:lang=\"en-US\"");
    append_buffer(" xmlns=\"" + namespaces.at("3mf") + "\"");
    append_buffer(" xmlns:m=\"" + namespaces.at("m") + "\"");
    append_buffer(" xmlns:slic3r=\"" + namespaces.at("slic3r") + "\"> \n");

    // Write metadata.
    write_metadata();

    // Write resources.
    append_buffer("    <resources> \n");

    // Write Model Material.
    write_materials();

    // Write Object
    for(size_t object_index = 0; object_index < model->objects.size(); object_index++)
        write_object(object_index);

    // Close resources
    append_buffer("    </resources> \n");

    // Write build element.
    write_build();

    // Close the model element.
    append_buffer("</model>\n");

    // Write what is found in the buffer.
    write_buffer();

    // Close the 3dmodel.model file in /3D/ directory
    if(zip_entry_close(zip_archive))
        return false;

    return true;
}

bool
TMFEditor::write_metadata()
{
    // Write the model metadata.
    for(std::map<std::string, std::string>::reverse_iterator  it = model->metadata.rbegin(); it != model->metadata.rend(); ++it){
        append_buffer("    <metadata name=\"" + it->first + "\">" + it->second + "</metadata>\n" );
    }

    // Write Slic3r metadata carrying the version number.
    append_buffer("    <slic3r:metadata type=\"version\">" + to_string(SLIC3R_VERSION) + "</slic3r:metadata>\n");

    return true;
}

bool
TMFEditor::write_materials()
{
    if (model->materials.size() == 0)
        return true;
    // Add materials into groups.
    std::map<int, std::vector<t_model_material_id>> material_groups;
    for (const auto &material : model->materials){
        if (material.first.empty())
            continue;
        int group_saved_id = material.second->material_group_id;
        // Check for existance of the material group in material groups map.
        if( material_groups.count(group_saved_id) <= 0) {
            material_groups_ids[group_saved_id] = material_group_id++;
        }
        material_groups[group_saved_id].push_back(material.first);
    }

    // Write material groups.
    for(const auto material_group : material_groups){
        int group_type = model->materials[material_group.second.front()]->material_group_type;
        switch (group_type){
            case static_cast<int>(TMFMaterialGroups::UNKNOWEN):
            case static_cast<int>(TMFMaterialGroups::BASE_MATERIAL): {
                int material_index = 0;
                std::map<t_model_material_id, int> material_group_index;
                // Write the base materials group.
                append_buffer(
                        "    <basematerials id=\"" + to_string(material_groups_ids[material_group.first]) + "\">\n");
                for (const auto material_id : material_group.second) {
                    // If id is empty ignore.
                    if (material_id.empty())
                        continue;
                    ModelMaterial *material = model->materials[material_id];
                    // Change the material index in its group.
                    material_group_index[material_id] = material_index++;

                    // We add it with black color by default.
                    append_buffer("        <base name=\"" + material->attributes["name"] + "\" ");

                    // If "displaycolor" attribute is not found, add a default black colour. Color is a must in base material in 3MF.
                    append_buffer("displaycolor=\"" +
                                  (material->attributes.count("displaycolor") > 0 ? material->attributes["displaycolor"]
                                                                                  : "#FFFFFFFF") + "\"/>\n");
                    // ToDo @Samir55 to be covered in AMF write .
                }
                append_buffer("    </basematerials>\n");

                // Write Slic3r custom config data group.
                // It has the following structure:
                // 1. All Slic3r metadata configs are stored in <Slic3r:materials> element which contains
                // <Slic3r:material> element having the following attributes:
                // material id "mid" it points to, type and then the serialized key.

                // Add Slic3r material config group.
                append_buffer("    <slic3r:materials>\n");

                for (const auto material_id : material_group.second) {
                    // If id is empty ignore.
                    if (material_id.empty())
                        continue;
                    ModelMaterial *material = model->materials[material_id];
                    for (const std::string &key : material->config.keys()) {
                        append_buffer("        <slic3r:material pid=\"" + to_string(material_groups_ids[material_group.first])
                                      + "\" pindex=\"" + to_string(material_group_index[material_id])
                                      + "\" type=\"" + key
                                      + "\" config=\"" + material->config.serialize(key) + "\">"
                        );
                        std::cout << "        <slic3r:material pid=\"" + to_string(material_groups_ids[material_group.first])
                                     + "\" pindex=\"" + to_string(material_group_index[material_id])
                                     + "\" type=\"" + key
                                     + "\" config=\"" + material->config.serialize(key) + "\">";
                    }
                }
                // close Slic3r material config group.
                append_buffer("    </slic3r:materials>\n");
            }
                break;
            case static_cast<int>(TMFMaterialGroups::COLOR):
                break;
            default:
                break;
        }
    }

    object_id += material_group_id - 1;
    return true;

}

bool
TMFEditor::write_object(int index)
{
    ModelObject* object = model->objects[index];

    // Create the new object element.
    append_buffer("        <object id=\"" + to_string(index + object_id++) + "\" type=\"model\"");

    // Add part number if found.
    if (object->part_number != -1)
        append_buffer(" partnumber=\"" + to_string(object->part_number) + "\"");

    append_buffer(">\n");

    // Write Slic3r custom configs.
    for (const std::string &key : object->config.keys()){
        append_buffer("        <slic3r:object type=\"" + key
                      + "\" config=\"" + object->config.serialize(key) + "\"" + "/>\n");
    }

    // Create mesh element which contains the vertices and the volumes.
    append_buffer("            <mesh>\n");

    // Create vertices element.
    append_buffer("                <vertices>\n");

    // Save the start offset of each volume vertices in the object.
    std::vector<size_t> vertices_offsets;
    size_t num_vertices = 0;

    for (ModelVolume* volume : object->volumes){
        // Require mesh vertices.
        volume->mesh.require_shared_vertices();

        vertices_offsets.push_back(num_vertices);
        const auto &stl = volume->mesh.stl;
        for (int i = 0; i < stl.stats.shared_vertices; ++i)
        {

            // Subtract origin_translation in order to restore the coordinates of the parts
            // before they were imported. Otherwise, when this 3MF file is reimported parts
            // will be placed in the platter correctly, but we will have lost origin_translation
            // thus any additional part added will not align with the others.
            // In order to do this we compensate for this translation in the instance placement
            // below.
            append_buffer("                    <vertex");
            append_buffer(" x=\"" + to_string(stl.v_shared[i].x - object->origin_translation.x) + "\"");
            append_buffer(" y=\"" + to_string(stl.v_shared[i].y - object->origin_translation.y) + "\"");
            append_buffer(" z=\"" + to_string(stl.v_shared[i].z - object->origin_translation.z) + "\"/>\n");
        }
        num_vertices += stl.stats.shared_vertices;
    }

    // Close the vertices element.
    append_buffer("                </vertices>\n");

    // Append volumes in triangles element.
    append_buffer("                <triangles>\n");

    // Save the start offset (triangle offset) of each volume (To be saved for writing Slic3r custom configs).
    std::vector<size_t> triangles_offsets;
    size_t num_triangles = 0;

    for (size_t i_volume = 0; i_volume < object->volumes.size(); ++i_volume) {
        ModelVolume *volume = object->volumes[i_volume];
        int vertices_offset = vertices_offsets[i_volume];
        triangles_offsets.push_back(num_triangles);

        // Add the volume triangles to the triangles list.
        for (int i = 0; i < volume->mesh.stl.stats.number_of_facets; ++i){
            append_buffer("                    <triangle");
            for (int j = 0; j < 3; j++){
                append_buffer(" v" + to_string(j+1) + "=\"" + to_string(volume->mesh.stl.v_indices[i].vertex[j] + vertices_offset) + "\"");
            }
            if (!volume->material_id().empty())
                append_buffer(" pid=\"1\" p1=\"" + to_string(volume->material_id()) + "\""); // Base Materials id = 1 and p1 is assigned to the whole triangle. ToDo @Samir55 Finish that.
            append_buffer("/>\n");
            num_triangles++;
        }
    }

    // Close the triangles element
    append_buffer("                </triangles>\n");

    // Add Slic3r volumes group.
    append_buffer("                <slic3r:volumes>\n");

    // Add each volume as <slic3r:volume> element containing Slic3r custom configs.
    // Each volume has the following attributes:
    // ts : "start triangle index", te : "end triangle index".

    for (size_t i_volume = 0; i_volume < object->volumes.size(); ++i_volume) {
        ModelVolume *volume = object->volumes[i_volume];

        append_buffer("                    <slic3r:volume ts=\"" + to_string(triangles_offsets[i_volume]) + "\""
                      + " te=\"" + ((i_volume + 1 == object->volumes.size()) ? to_string(int(num_triangles)-1) : to_string(triangles_offsets[i_volume+1] - 1)) + "\""
                      + (volume->modifier ? " modifier=\"1\" " : " modifier=\"0\" ")
                      + ">\n");

        for (const std::string &key : volume->config.keys()){
            append_buffer("                        <slic3r:metadata type=\"" +  key
                          + "\" config=\"" + volume->config.serialize(key) + "/>\n");
        }

        // Close Slic3r volume
        append_buffer("                    </slic3r:volume>\n");
    }

    // Close Slic3r volumes group.
    append_buffer("                </slic3r:volumes>\n");

    // Close the mesh element.
    append_buffer("            </mesh>\n");

    // Close the object element.
    append_buffer("        </object>\n");

    return true;
}

bool
TMFEditor::write_build()
{
    // Create build element.
    append_buffer("    <build> \n");

    // Write ModelInstances for each ModelObject.
    for(size_t object_id = 0; object_id < model->objects.size(); ++object_id){
        ModelObject* object = model->objects[object_id];
        for (const ModelInstance* instance : object->instances){
            append_buffer("        <item objectid=\"" + to_string(object_id + 1) + "\"");

            // Get the rotation about z and the scale factor.
            double sc = instance->scaling_factor, cosine_rz = cos(instance->rotation) , sine_rz = sin(instance->rotation);
            double tx = instance->offset.x + object->origin_translation.x , ty = instance->offset.y + object->origin_translation.y;

            std::string transform = to_string(cosine_rz * sc) + " " + to_string(sine_rz * sc) + " 0 "
                                    + to_string(-1 * sine_rz * sc) + " " + to_string(cosine_rz*sc) + " 0 "
                                    + "0 " + "0 " + to_string(sc) + " "
                                    + to_string(tx) + " " + to_string(ty) + " " + "0";

            // Add the transform
            append_buffer(" transform=\"" + transform + "\"/>\n");

        }
    }
    append_buffer("    </build> \n");
    return true;
}

bool
TMFEditor::read_model()
{
    // Extract 3dmodel.model entry.
    zip_entry_open(zip_archive, "3D/3dmodel.model");
    zip_entry_fread(zip_archive, "3dmodel.model");
    zip_entry_close(zip_archive);

    // Read 3D/3dmodel.model file.
    XML_Parser parser = XML_ParserCreate(NULL);
    if (! parser) {
        printf("Couldn't allocate memory for parser\n");
        return false;
    }

    boost::nowide::ifstream fin("3dmodel.model", std::ios::in);
    if (!fin.is_open()) {
        boost::nowide::cerr << "Cannot open file: " << "3dmodel.model" << std::endl;
        return false;
    }

    // Create model parser.
    TMFParserContext ctx(parser, model);
    XML_SetUserData(parser, (void*)&ctx);
    XML_SetElementHandler(parser, TMFParserContext::startElement, TMFParserContext::endElement);
    XML_SetCharacterDataHandler(parser, TMFParserContext::characters);

    char buff[8192];
    bool result = false;
    while (!fin.eof()) {
        fin.read(buff, sizeof(buff));
        if (fin.bad()) {
            printf("3MF model parser: Read error\n");
            break;
        }
        if (XML_Parse(parser, buff, fin.gcount(), fin.eof()) == XML_STATUS_ERROR) {
            printf("3MF model parser: Parse error at line %lu:\n%s\n",
                   XML_GetCurrentLineNumber(parser),
                   XML_ErrorString(XML_GetErrorCode(parser)));
            break;
        }
        if (fin.eof()) {
            result = true;
            break;
        }
    }

    // Free the parser and close the file.
    XML_ParserFree(parser);
    fin.close();

    // Remove the extracted 3dmodel.model file.
    if (remove("3dmodel.model") != 0)
        return false;

    if (result)
        ctx.endDocument();
    return result;
}

bool
TMFEditor::produce_TMF()
{
    // Create a new zip archive object.
    zip_archive = zip_open(zip_name.c_str(), ZIP_DEFLATE_COMPRESSION, 'w');

    // Check whether it's created or not.
    if(!zip_archive) return false;

    // Prepare the 3MF Zip archive by writing the relationships.
    if(!write_relationships())
        return false;

    // Prepare the 3MF Zip archive by writing the types.
    if(!write_types())
        return false;

    // Write the model.
    if(!write_model()) return false;

    // Finalize the archive and end writing.
    zip_close(zip_archive);
    return true;
}

bool
TMFEditor::consume_TMF()
{
    // Open the 3MF package.
    zip_archive = zip_open(zip_name.c_str(), 0, 'r');

    // Check whether it's opened or not.
    if(!zip_archive) return false;

    // Read model.
    if(!read_model())
        return false;

    // Close zip archive.
    zip_close(zip_archive);
    return true;
}

void
TMFEditor::append_buffer(std::string s)
{
    buff += s;
    if(buff.size() + s.size() > WRITE_BUFFER_MAX_CAPACITY)
        write_buffer();
}

void
TMFEditor::write_buffer()
{
    // Append the current opened entry with the current buffer.
    zip_entry_write(zip_archive, buff.c_str(), buff.size());
    // Clear the buffer.
    buff = "";
}

TMFEditor::~TMFEditor()
{

}

bool
TMF::write(Model& model, std::string output_file)
{
    TMFEditor tmf_writer(output_file, &model);
    return tmf_writer.produce_TMF();
}

bool
TMF::read(std::string input_file, Model* model)
{
    TMFEditor tmf_reader(input_file, model);
    return tmf_reader.consume_TMF();
}

TMFParserContext::TMFParserContext(XML_Parser parser, Model *model):
        m_parser(parser),
        m_model(*model),
        m_object(NULL),
        m_volume(NULL),
        m_material(NULL)
{
    m_path.reserve(9);
}

void XMLCALL
TMFParserContext::startElement(void *userData, const char *name, const char **atts){
    TMFParserContext *ctx = (TMFParserContext*) userData;
    ctx->startElement(name, atts);
}

void XMLCALL
TMFParserContext::endElement(void *userData, const char *name)
{
    TMFParserContext *ctx = (TMFParserContext*)userData;
    ctx->endElement(name);
}

void XMLCALL
TMFParserContext::characters(void *userData, const XML_Char *s, int len)
{
    TMFParserContext *ctx = (TMFParserContext*)userData;
    ctx->characters(s, len);
}

const char*
TMFParserContext::get_attribute(const char **atts, const char *id) {
    if (atts == NULL)
        return NULL;
    while (*atts != NULL) {
        if (strcmp(*(atts ++), id) == 0)
            return *atts;
        ++ atts;
    }
    return NULL;
}

void
TMFParserContext::startElement(const char *name, const char **atts)
{
    TMFNodeType node_type_new = NODE_TYPE_UNKNOWN;
    switch (m_path.size()){
        case 0:
            // Must be <model> tag.
            if (strcmp(name, "model") != 0)
                this->stop();
            node_type_new = NODE_TYPE_MODEL;
            break;
        case 1:
            if (strcmp(name, "metadata") == 0) {
                const char* metadata_name = this->get_attribute(atts, "name");
                // Name is required if it's not found stop parsing.
                if (!metadata_name)
                    this->stop();
                m_value[0] = metadata_name;
                node_type_new = NODE_TYPE_METADATA;
            } else if (strcmp(name, "resources") == 0) {
                node_type_new = NODE_TYPE_RESOURCES;
            } else if (strcmp(name, "build") == 0) {
                node_type_new = NODE_TYPE_BUILD;
            }
            break;
        case 2:
            if (strcmp(name, "basematerials") == 0) {
                if (!this->read_material_group(atts, TMFMaterialGroups::BASE_MATERIAL))
                    this->stop();

                node_type_new = NODE_TYPE_BASE_MATERIALS;
            } else if (strcmp(name, "m:colorgroup") == 0) {
                if (!this->read_material_group(atts, TMFMaterialGroups::COLOR))
                    this->stop();

                node_type_new = NODE_TYPE_COLOR_GROUP;

            } else if (strcmp(name, "slic3r:materials") == 0) {
                node_type_new = NODE_TYPE_SLIC3R_MATERIALS;
            } else if (strcmp(name, "object") == 0){
                const char* object_id = get_attribute(atts, "id");
                if (!object_id)
                    this->stop();

                if(!m_object_vertices.empty())
                    this->stop();

                // Create a new object in the model. This object should be included in another object if
                // it's a component in another object.
                m_object = m_model.add_object();
                m_objects_indices[object_id] = m_model.objects.size() - 1;
                m_output_objects.push_back(1); // default value 1 means: it's must not be an output.

                // Add part number.
                const char* part_number = get_attribute(atts, "partnumber");
                m_object->part_number = (!part_number) ? -1 : atoi(part_number);

                // Add object name.
                const char*  object_name = get_attribute(atts, "name");
                m_object->name = (!object_name) ? "" : object_name;

                // Add the object material if applicable.
                const char* property_group_id = get_attribute(atts, "pid");
                const char* property_index = get_attribute(atts, "pindex");
                if (property_group_id && property_index) {
                    m_object_material_group_id = property_group_id;
                    m_object_material_id = property_index;
                }
                node_type_new = NODE_TYPE_OBJECT;
            } else if (strcmp(name, "item") == 0){
                // Get object id.
                const char* object_id = get_attribute(atts, "objectid");
                if(!object_id)
                    this->stop();
                // Mark object as output.
                m_output_objects[m_objects_indices[object_id]] = 0;

                // Add instance.
                ModelInstance* instance = m_model.objects[m_objects_indices[object_id]]->add_instance();

                // Apply transformation if supplied.
                const char* transformation_matrix = get_attribute(atts, "transform");
                if(transformation_matrix){
                    // Decompose the affine matrix.
                    std::vector<double> transformations;
                    if(!get_transformations(transformation_matrix, transformations))
                        this->stop();

                    if(transformations.size() != 9)
                        this->stop();

                    apply_transformation(instance, transformations);

                }

                node_type_new = NODE_TYPE_ITEM;
            }
            break;
        case 3:
            if (strcmp(name, "base") == 0) {
                if(!this->read_material(atts))
                    this->stop();

                node_type_new = NODE_TYPE_BASE;
            } else if (strcmp(name, "m:color") == 0){
                if(!this->read_material(atts))
                    this->stop();

                node_type_new = NODE_TYPE_COLOR;
            } else if (strcmp(name, "slic3r:material") == 0) {
                // Get the material group id.
                const char* property_group_id = get_attribute(atts, "pid");

                // Get the material index.
                const char* property_index = get_attribute(atts, "pindex");

                // Get the option key.
                const char* key = get_attribute(atts, "type");

                // Get the config value.
                const char* config_value = get_attribute(atts, "config");

                // Get the material.
                std::pair<int, int> material_index(atoi(property_group_id), atoi(property_index));
                ModelMaterial* material = m_model.materials[material_groups[material_index]];

                DynamicPrintConfig* config = &material->config;
                if (config && print_config_def.options.find(key) != print_config_def.options.end() )
                    config->set_deserialize(key, config_value);

                node_type_new = NODE_TYPE_SLIC3R_MATERIAL;
            } else if (strcmp(name, "mesh") == 0){
                // Create a new model volume.
                if(m_volume)
                    this->stop();
                node_type_new = NODE_TYPE_MESH;
            } else if (strcmp(name, "components") == 0) {
                node_type_new = NODE_TYPE_COMPONENTS;
            } else if (strcmp(name, "slic3r:object")) {
                node_type_new = NODE_TYPE_SLIC3R_OBJECT_CONFIG;

                // Create a config option.
                DynamicPrintConfig *config = NULL;
                if(m_path.back() == NODE_TYPE_OBJECT && m_object)
                    config = &m_object->config;

                // Get the config key type.
                const char *key = get_attribute(atts, "type");

                if (config && print_config_def.options.find(key) != print_config_def.options.end() ) {
                    // Get the key config string.
                    const char *config_value = get_attribute(atts, "config");

                    config->set_deserialize(key, config_value);
                }
            }
            break;
        case 4:
            if (strcmp(name, "vertices") == 0) {
                node_type_new = NODE_TYPE_VERTICES;
            } else if (strcmp(name, "triangles") == 0) {
                node_type_new = NODE_TYPE_TRIANGLES;
            } else if (strcmp(name, "component") == 0) {
                // Read the object id.
                const char* object_id = get_attribute(atts, "objectid");
                if(!object_id)
                    this->stop();
                ModelObject* component_object = m_model.objects[m_objects_indices[object_id]];
                // Append it to the parent (current m_object) as a mesh since Slic3r doesn't support an object inside another.
                // after applying 3d matrix transformation if found.
                TriangleMesh component_mesh;
                const char* transformation_matrix = get_attribute(atts, "transform");
                if(transformation_matrix){
                    // Decompose the affine matrix.
                    std::vector<double> transformations;
                    if(!get_transformations(transformation_matrix, transformations))
                        this->stop();

                    if( transformations.size() != 9)
                        this->stop();

                    // Create a copy of the current object.
                    ModelObject* object_copy = m_model.add_object(*component_object, true);

                    apply_transformation(object_copy, transformations);

                    // Get the mesh of this instance object.
                    component_mesh = object_copy->raw_mesh();

                    // Delete the copy of the object.
                    m_model.delete_object(m_model.objects.size() - 1);

                } else {
                    component_mesh = component_object->raw_mesh();
                }
                ModelVolume* volume = m_object->add_volume(component_mesh);
                if(!volume)
                    this->stop();
            } else if (strcmp(name, "slic3r:volumes") == 0) {
                node_type_new = NODE_TYPE_SLIC3R_VOLUMES;
            }
            break;
        case 5:
            if (strcmp(name, "vertex") == 0) {
                const char* x = get_attribute(atts, "x");
                const char* y = get_attribute(atts, "y");
                const char* z = get_attribute(atts, "z");
                if ( !x || !y || !z)
                    this->stop();
                m_object_vertices.push_back(atof(x));
                m_object_vertices.push_back(atof(y));
                m_object_vertices.push_back(atof(z));
                node_type_new = NODE_TYPE_VERTEX;
            } else if (strcmp(name, "triangle") == 0) {
                const char* v1 = get_attribute(atts, "v1");
                const char* v2 = get_attribute(atts, "v2");
                const char* v3 = get_attribute(atts, "v3");
                if (!v1 || !v2 || !v3)
                    this->stop();
                // Add it to the volume facets.
                m_volume_facets.push_back(atoi(v1));
                m_volume_facets.push_back(atoi(v2));
                m_volume_facets.push_back(atoi(v3));
                node_type_new = NODE_TYPE_TRIANGLE;
            } else if (strcmp(name, "slic3r:volume") == 0) {
                node_type_new = NODE_TYPE_SLIC3R_VOLUME;
                // Read start offset of the triangles.
                m_value[0] = get_attribute(atts, "ts");
                m_value[1] = get_attribute(atts, "te");
                m_value[2] = get_attribute(atts, "modifier");
                if( m_value[0].empty() || m_value[1].empty() || m_value[2].empty())
                    this->stop();
                // Add a new volume to the current object.
                if(!m_object)
                    this->stop();
                //m_volume = add_volume(stoi(m_value[0])*3, stoi(m_value[1]) * 3 + 2, stoi(m_value[2])); ToDo @Samir Fix slicer:volume seg. fault
//                if(!m_volume)
//                    this->stop();
            }
            break;
        case 6:
            if( strcmp(name, "slic3r:metadata") == 0){
                node_type_new = NODE_TYPE_SLIC3R_METADATA;
                // Create a config option.
                DynamicPrintConfig *config = NULL;
                if(!m_volume)
                    this->stop();
                config = &m_volume->config;
                const char *key = get_attribute(atts, "type");
                if( config && print_config_def.options.find(key) != print_config_def.options.end()){
                    const char *config_value = get_attribute(atts, "config");
                    config->set_deserialize(key, config_value);
                }
            }
        default:
            break;
    }

    m_path.push_back(node_type_new);
}

void
TMFParserContext::endElement(const char *name)
{
    switch (m_path.back()){
        case NODE_TYPE_METADATA:
            if( m_path.size() == 2) {
                m_model.metadata[m_value[0]] = m_value[1];
                m_value[1].clear();
            }
            break;
        case NODE_TYPE_MESH:
            // Add the object volume if no there are no added volumes in slic3r:volumes.
            if(m_object->volumes.size() == 0) {
                if(!m_object)
                    this->stop();
                m_volume = add_volume(0, m_volume_facets.size() - 1, 0);
                if (!m_volume)
                    this->stop();
                m_volume = NULL;
            }
            break;
        case NODE_TYPE_OBJECT:
            if(!m_object)
                this->stop();
            m_object_vertices.clear();
            m_volume_facets.clear();
            m_object = NULL;
            break;
        case NODE_TYPE_MODEL:
        {
            size_t deleted_objects_count = 0;
            // According to 3MF spec. we must output objects found in item.
            for (size_t i = 0; i < m_output_objects.size(); i++) {
                if (m_output_objects[i]) {
                    m_model.delete_object(i - deleted_objects_count);
                    deleted_objects_count++;
                }
            }
        }
            break;
        case NODE_TYPE_SLIC3R_VOLUME:
            m_volume = NULL;
            m_value[0].clear();
            m_value[1].clear();
            m_value[2].clear();
            break;
        default:
            break;
    }

    m_path.pop_back();
}

void
TMFParserContext::characters(const XML_Char *s, int len)
{
    switch (m_path.back()) {
        case NODE_TYPE_METADATA:
            if(m_path.size() == 2)
            m_value[1].append(s, len);
            break;
        default:
            break;
    }
}

void
TMFParserContext::endDocument()
{

}

void
TMFParserContext::stop()
{
    XML_StopParser(m_parser, 0);
}

bool
TMFParserContext::get_transformations(std::string matrix, std::vector<double> &transformations)
{
    // Get the values.
    double m[12];
    int  k = 0;
    std::string tmp = "";
    for (size_t i= 0; i < matrix.size(); i++)
        if ((matrix[i] == ' ' && !tmp.empty()) || (i == matrix.size() - 1 && !tmp.empty())) {
            m[k++] = std::stof(tmp);
            tmp = "";
        }else
            tmp += matrix[i];
    if(tmp != "")
        m[k++] = std::stof(tmp);

    if(k != 12)
        return false;

    // Get the translation (x,y,z) value. Remember the matrix in 3mf is a row major not a column major.
    transformations.push_back(m[9]);
    transformations.push_back(m[10]);
    transformations.push_back(m[11]);

    // Get the scale values.
    double sx = sqrt( m[0] * m[0] + m[1] * m[1] + m[2] * m[2]),
        sy = sqrt( m[3] * m[3] + m[4] * m[4] + m[5] * m[5]),
        sz = sqrt( m[6] * m[6] + m[7] * m[7] + m[8] * m[8]);
    transformations.push_back(sx);
    transformations.push_back(sy);
    transformations.push_back(sz);

    // Get the rotation values.

    // Normalize scale from the rotation matrix.
    m[0] /= sx; m[1] /= sy; m[2] /= sz;
    m[3] /= sx; m[4] /= sy; m[5] /= sz;
    m[6] /= sx; m[7] /= sy; m[8] /= sz;

    // Get quaternion values
    double q_w = sqrt(std::max(0.0, 1.0 + m[0] + m[4] + m[8])) / 2,
        q_x = sqrt(std::max(0.0, 1.0 + m[0] - m[4] - m[8])) / 2,
        q_y = sqrt(std::max(0.0, 1.0 - m[0] + m[4] - m[8])) / 2,
        q_z = sqrt(std::max(0.0, 1.0 - m[0] - m[4] + m[8])) / 2;

    q_x *= ((q_x * (m[5] - m[7])) <= 0 ? -1 : 1);
    q_y *= ((q_y * (m[6] - m[2])) <= 0 ? -1 : 1);
    q_z *= ((q_z * (m[1] - m[3])) <= 0 ? -1 : 1);

    // Normalize quaternion values.
    double q_magnitude = sqrt(q_w * q_w + q_x * q_x + q_y * q_y + q_z * q_z);
    q_w /= q_magnitude;
    q_x /= q_magnitude;
    q_y /= q_magnitude;
    q_z /= q_magnitude;

    double test = q_x * q_y + q_z * q_w;
    double result_x, result_y, result_z;
    // singularity at north pole
    if (test > 0.499)
    {
        result_x = 0;
        result_y = 2 * atan2(q_x, q_w);
        result_z = PI / 2;
    }
        // singularity at south pole
    else if (test < -0.499)
    {
        result_x = 0;
        result_y = -2 * atan2(q_x, q_w);
        result_z = -PI / 2;
    }
    else
    {
        result_x = atan2(2 * q_x * q_w - 2 * q_y * q_z, 1 - 2 * q_x * q_x - 2 * q_z * q_z);
        result_y = atan2(2 * q_y * q_w - 2 * q_x * q_z, 1 - 2 * q_y * q_y - 2 * q_z * q_z);
        result_z = asin(2 * q_x * q_y + 2 * q_z * q_w);

        if (result_x < 0) result_x += 2 * PI;
        if (result_y < 0) result_y += 2 * PI;
        if (result_z < 0) result_z += 2 * PI;
    }
    transformations.push_back(result_x);
    transformations.push_back(result_y);
    transformations.push_back(result_z);

    return true;
}

void
TMFParserContext::apply_transformation(ModelObject *object, std::vector<double> &transformations)
{
    // Apply scale.
    Pointf3 vec(transformations[3], transformations[4], transformations[5]);
    object->scale(vec);

    // Apply x, y & z rotation.
    object->rotate(transformations[6], X);
    object->rotate(transformations[7], Y);
    object->rotate(transformations[8], Z);

    // Apply translation.
    object->translate(transformations[0], transformations[1], transformations[2]);
    return;
}

void
TMFParserContext::apply_transformation(ModelInstance *instance, std::vector<double> &transformations)
{
    // Apply scale.
    instance->scaling_vector = Pointf3(transformations[3], transformations[4], transformations[5]);;

    // Apply x, y & z rotation.
    instance->rotation = transformations[8];
    instance->x_rotation = transformations[6];
    instance->y_rotation = transformations[7];

    // Apply translation.
    instance->offset.x = transformations[0] < 0 ? 0: transformations[0];
    instance->offset.y = transformations[1] < 0 ? 0: transformations[1];
    instance->z_translation = transformations[2] < 0 ? 0: transformations[2];
    return;
}

ModelVolume*
TMFParserContext::add_volume(int start_offset, int end_offset, bool modifier, t_model_material_id material_id)
{
    ModelVolume* m_volume = NULL;

    // Add a new volume.
    m_volume = m_object->add_volume(TriangleMesh());
    if(!m_volume || (end_offset < start_offset)) return NULL;

    // Add the triangles.
    stl_file &stl = m_volume->mesh.stl;
    stl.stats.type = inmemory;
    stl.stats.number_of_facets = (1 + end_offset - start_offset) / 3;
    stl.stats.original_num_facets = stl.stats.number_of_facets;
    stl_allocate(&stl);
    for (int i = start_offset; i <= end_offset ;) {
        stl_facet &facet = stl.facet_start[i / 3];
        for (unsigned int v = 0; v < 3; ++v)
            memcpy(&facet.vertex[v].x, &m_object_vertices[m_volume_facets[i++] * 3], 3 * sizeof(float));
    }
    stl_get_size(&stl);
    m_volume->mesh.repair();
    m_volume->modifier = modifier;

    if(!material_id.empty())
        m_volume->set_material(material_id, *m_model.materials[material_id]);
    return m_volume;
}

bool
TMFParserContext::read_material_group(const char** atts, TMFMaterialGroups group_type) {

    // Read the current property group id.
    const char* property_group_id = this->get_attribute(atts,"id");

    // Property id is a required field.
    if (!property_group_id)
        return false;

    this->material_group_id = std::stoi(property_group_id);
    this->material_group_type = static_cast<int>(group_type);
    this->used_material_groups.push_back(0);

    return true;
}

bool
TMFParserContext::read_material(const char **atts) {
    // Create a new model material.
    this->m_material = m_model.add_material(std::to_string(m_model.materials.size()));

    if(!(this->m_material))
        return false;

    // Add the material group number and group type.
    this->m_material->material_group_id = material_group_id;
    this->m_material->material_group_type = material_group_type;

    // Add the model material attributes.
    while(*atts != NULL){
        this->m_material->attributes[*(atts)] = *(atts + 1);
        atts += 2;
    }

    return true;
}

} }