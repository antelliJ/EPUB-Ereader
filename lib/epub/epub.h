#pragma once

// Right now just worrying about the spine, not the TOC, but we can add that later
#include <string>
#include <vector>
#include <unordered_map>
#ifndef UNIT_TEST
  #include <freertos/FreeRTOS.h>
  #include <freertos/task.h>
#endif



class ZipFile;

class EpubTocEntry
{
public:
  std::string title;
  std::string href;
  std::string anchor;
  int level;
  EpubTocEntry(std::string title, std::string href, std::string anchor, int level) : title(title), href(href), anchor(anchor), level(level) {}
};

class Epub
{
private:
  // the title read from the EPUB meta data
  std::string m_title;
  // the cover image
  std::string m_cover_image_item;
  // the ncx file
  std::string m_toc_ncx_item;
  // where is the EPUBfile?
  std::string m_path;
  // the spine of the EPUB file
  std::vector<std::pair<std::string, std::string>> m_spine;
  // the toc of the EPUB file
  std::vector<EpubTocEntry> m_toc;
  // the base path for items in the EPUB file
  std::string m_base_path;
  // find the path for the content.opf file
  bool find_content_opf_file(ZipFile &zip, std::string &content_opf_file);
  bool parse_content_opf(ZipFile &zip, std::string &content_opf_file);
  bool parse_toc_ncx_file(ZipFile &zip);

public:
  Epub(const std::string &path);
  ~Epub() {}
  std::string &get_base_path() { return m_base_path; }
  bool load();

  const std::string &get_path() const { return m_path; }
  const std::string &get_title();
  const std::string &get_cover_image_item();
  uint8_t *get_item_contents(const std::string &item_href, size_t *size = nullptr);

  std::string &get_spine_item(int spine_index);
  int get_spine_item_id(std::string spine_key);
  int get_spine_items_count();

  EpubTocEntry &get_toc_item(int toc_index);
  int get_toc_items_count();
  // work out the section index for a toc index
  int get_spine_index_for_toc_index(int toc_index);
};

std::string normalise_path(const std::string &path)
{
  std::vector<std::string> components;
  std::string component;
  for (auto c : path)
  {
    if (c == '/')
    {
      if (!component.empty())
      {
        if (component == "..")
        {
          if (!components.empty())
          {
            components.pop_back();
          }
        }
        else
        {
          components.push_back(component);
        }
        component.clear();
      }
    }
    else
    {
      component += c;
    }
  }
  if (!component.empty())
  {
    components.push_back(component);
  }
  std::string result;
  for (auto &component : components)
  {
    if (result.size() > 0)
    {
      result += "/";
    }
    result += component;
  }
  return result;
}

#include <esp_log.h>


#include <map>
#include "tinyxml2.h"
#include "../ZipFile/ZipFile.h"
#include "Epub.h"

static const char *TAG = "EPUB";
// static char *TAG = "EPUB";

bool Epub::find_content_opf_file(ZipFile &zip, std::string &content_opf_file)
{
  // open up the meta data to find where the content.opf file lives
  char *meta_info = (char *)zip.read_file_to_memory2("META-INF/container.xml");
  if (!meta_info)
  {
    ESP_LOGE(TAG, "Could not find META-INF/container.xml");
    return false;
  }
  // parse the meta data
  tinyxml2::XMLDocument meta_data_doc;
  auto result = meta_data_doc.Parse(meta_info);
  // finished with the data as it's been parsed
  free(meta_info);
  if (result != tinyxml2::XML_SUCCESS)
  {
    ESP_LOGE(TAG, "Could not parse META-INF/container.xml");
    return false;
  }
  auto container = meta_data_doc.FirstChildElement("container");
  if (!container)
  {
    ESP_LOGE(TAG, "Could not find container element in META-INF/container.xml");
    return false;
  }
  auto rootfiles = container->FirstChildElement("rootfiles");
  if (!rootfiles)
  {
    ESP_LOGE(TAG, "Could not find rootfiles element in META-INF/container.xml");
    return false;
  }
  // find the root file that has the media-type="application/oebps-package+xml"
  auto rootfile = rootfiles->FirstChildElement("rootfile");
  while (rootfile)
  {
    const char *media_type = rootfile->Attribute("media-type");
    if (media_type && strcmp(media_type, "application/oebps-package+xml") == 0)
    {
      const char *full_path = rootfile->Attribute("full-path");
      if (full_path)
      {
        content_opf_file = full_path;
        return true;
      }
    }
    rootfile = rootfile->NextSiblingElement("rootfile");
  }
  ESP_LOGE(TAG, "Could not get path to content.opf file");
  return false;
}

bool Epub::parse_content_opf(ZipFile &zip, std::string &content_opf_file)
{
  // read in the content.opf file and parse it
  char *contents = (char *)zip.read_file_to_memory2(content_opf_file.c_str());
  // parse the contents
  // tinyxml2::XMLDocument doc;
  // auto result = doc.Parse(contents);
  auto doc = std::unique_ptr<tinyxml2::XMLDocument>(new tinyxml2::XMLDocument());
  auto result = doc->Parse(contents);
  free(contents);
  if (result != tinyxml2::XML_SUCCESS)
  {
    ESP_LOGE(TAG, "Error parsing content.opf - %s", doc->ErrorIDToName(result));
    return false;
  }
  auto package = doc->FirstChildElement("package");
  if (!package)
  {
    ESP_LOGE(TAG, "Could not find package element in content.opf");
    return false;
  }
  // get the metadata - title and cover image
  auto metadata = package->FirstChildElement("metadata");
  if (!metadata)
  {
    ESP_LOGE(TAG, "Missing metadata");
    return false;
  }
  auto title = metadata->FirstChildElement("dc:title");
  if (!title)
  {
    ESP_LOGE(TAG, "Missing title");
    return false;
  }
  m_title = title->GetText();
  auto cover = metadata->FirstChildElement("meta");
  while (cover && cover->Attribute("name") && strcmp(cover->Attribute("name"), "cover") != 0)
  {
    cover = cover->NextSiblingElement("meta");
  }
  if (!cover)
  {
    ESP_LOGW(TAG, "Missing cover");
  }
  auto cover_item = cover ? cover->Attribute("content") : nullptr;
  // read the manifest and spine
  // the manifest gives us the names of the files
  // the spine gives us the order of the files
  // we can then read the files in the order they are in the spine
  auto manifest = package->FirstChildElement("manifest");
  if (!manifest)
  {
    ESP_LOGE(TAG, "Missing manifest");
    return false;
  }
  // create a mapping from id to file name
  auto item = manifest->FirstChildElement("item");
  std::map<std::string, std::string> items;

  while (item)
  {
    std::string item_id = item->Attribute("id");
    std::string href = m_base_path + item->Attribute("href");
    // grab the cover image
    if (cover_item && item_id == cover_item)
    {
      m_cover_image_item = href;
    }
    // grab the ncx file
    if (item_id == "ncx")
    {
      m_toc_ncx_item = href;
    }
    items[item_id] = href;
    item = item->NextSiblingElement("item");
  }
  // find the spine
  auto spine = package->FirstChildElement("spine");
  if (!spine)
  {
    ESP_LOGE(TAG, "Missing spine");
    return false;
  }
  // read the spine
  auto itemref = spine->FirstChildElement("itemref");
  while (itemref)
  {
    auto id = itemref->Attribute("idref");
    if (items.find(id) != items.end())
    {
      m_spine.push_back(std::make_pair(id, items[id]));
    }
    itemref = itemref->NextSiblingElement("itemref");
  }
  return true;
}


bool Epub::parse_toc_ncx_file(ZipFile &zip){
  // ncx file is specifified in content.opf
  if (m_toc_ncx_item.empty()){
    ESP_LOGW(TAG, "No ncx file specified in content.opf");
    return false;
  }

  char *ncx_data = (char *)zip.read_file_to_memory2(m_toc_ncx_item.c_str());
  if (!ncx_data){
    ESP_LOGE(TAG, "Could not read ncx file %s", m_toc_ncx_item.c_str());
    return false;
  }
  // parse the ncx file
  // tinyxml2::XMLDocument ncx_doc;
  // auto result = ncx_doc.Parse(ncx_data);
  auto ncx_doc = std::unique_ptr<tinyxml2::XMLDocument>(new tinyxml2::XMLDocument());
  auto result = ncx_doc->Parse(ncx_data);
  free(ncx_data);
  if (result != tinyxml2::XML_SUCCESS) {
    ESP_LOGE(TAG, "Error parsing ncx file - %s", ncx_doc->ErrorIDToName(result));
    return false;
  }
  auto ncx = ncx_doc->FirstChildElement("ncx");
  if (!ncx){
    ESP_LOGE(TAG, "Could not find ncx element in ncx file");
    return false;
  }

  auto navMap = ncx->FirstChildElement("navMap");
  if (!navMap){
    ESP_LOGE(TAG, "Could not find navMap element in ncx file");
    return false;
  }

  // read the navPoints
  auto navPoint = navMap->FirstChildElement("navPoint");
  // fille the toc_index map
  while (navPoint) {
    // there is id and playOrder attributes on the navPoint element, but we don't need them right now
    auto navLabel = navPoint->FirstChildElement("navLabel")->FirstChildElement("text")->FirstChild();
    Serial.printf("Parsing TOC entry: %s\n", navLabel ? navLabel->Value() : "Untitled");
    std::string title = navLabel ? navLabel->Value() : "Untitled";
    auto content = navPoint->FirstChildElement("content");
    std::string href = content ? content->Attribute("src") : "";
    // split the href on the # to get href and anchor separately
    size_t anchor_pos = href.find('#');
    // if there is no # in the href, then there is no anchor
    std::string anchor = "";

    if (anchor_pos != std::string::npos){
      anchor = href.substr(anchor_pos + 1);
      href = href.substr(0, anchor_pos);
    }
    m_toc.push_back(EpubTocEntry(title, href, anchor, 0));
    // check for child navPoints and add them to the toc with an incremented level
    navPoint = navPoint->NextSiblingElement("navPoint");
  }
  return true;
}

uint8_t *Epub::get_item_contents(const std::string &item_href, size_t *size)
{
  ZipFile zip(m_path.c_str());
  std::string path = normalise_path(item_href);
  Serial.printf("Getting item contents for %s\n", path.c_str());
  auto content = zip.read_file_to_memory2(path.c_str(), size);
  if (!content)
  {
    ESP_LOGE(TAG, "Failed to read item %s", path.c_str());
    return nullptr;
  }
  return content;
}

int Epub::get_spine_items_count()
{
  return m_spine.size();
}

std::string &Epub::get_spine_item(int spine_index)
{
  ESP_LOGD(TAG, "get_spine_item index:%d", spine_index);
  try
    {
      return m_spine.at(spine_index).second;
    } catch (const std::out_of_range &oor) {
      
      ESP_LOGI(TAG, "get_spine_item index:%d is out_of_range", spine_index);
      spine_index = 0;
    }
    // if exception is catched return last page
    return m_spine.at(spine_index).second;
}

Epub::Epub(const std::string &path) : m_path(path)
{
}

// load in the meta data for the epub file
bool Epub::load()
{
  ZipFile zip(m_path.c_str());
  std::string content_opf_file;
  if (!find_content_opf_file(zip, content_opf_file))
  {
    #ifndef UNIT_TEST
      ESP_LOGE(TAG, "Could not open ePub. Restarting in 10 secs.");
      vTaskDelay(pdMS_TO_TICKS(1000*10));
      esp_restart();
    #endif
    return false;
  }
  // get the base path for the content
  m_base_path = content_opf_file.substr(0, content_opf_file.find_last_of('/') + 1);
  if (!parse_content_opf(zip, content_opf_file))
  {
    return false;
  }
  if (!parse_toc_ncx_file(zip))
  {
    return false;
  }
  return true;
}

const std::string &Epub::get_title()
{
  return m_title;
}

EpubTocEntry &Epub::get_toc_item(int toc_index){
  return m_toc.at(toc_index);
}

int Epub::get_toc_items_count(){
  return m_toc.size();
}

// work out the section index for a toc index
int Epub::get_spine_index_for_toc_index(int toc_index){
  // toc entry should have an href that matches the spine item href
  // can find spine index by matching the href in the spine to the href in the toc entry
  for (int i = 0; i < m_spine.size(); i++){
    if (m_spine[i].second == m_toc[toc_index].href){
      return i;
    }
  }

  // default to first section if not found
  return 0;
}