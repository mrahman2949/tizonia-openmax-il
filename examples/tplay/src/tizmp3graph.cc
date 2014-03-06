/**
 * Copyright (C) 2011-2014 Aratelia Limited - Juan A. Rubio
 *
 * This file is part of Tizonia
 *
 * Tizonia is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Lesser General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Tizonia is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with Tizonia.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @file   tizmp3graph.cc
 * @author Juan A. Rubio <juan.rubio@aratelia.com>
 *
 * @brief  OpenMAX IL mp3 decoder graph implementation
 *
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <boost/bind.hpp>
#include <boost/make_shared.hpp>

#include <OMX_Core.h>
#include <OMX_Component.h>
#include <tizosal.h>

#include "tizgraphutil.h"
#include "tizgraphconfig.h"
#include "tizprobe.h"
#include "tizmp3graph.h"

#ifdef TIZ_LOG_CATEGORY_NAME
#undef TIZ_LOG_CATEGORY_NAME
#define TIZ_LOG_CATEGORY_NAME "tiz.play.graph.mp3decoder"
#endif

namespace graph = tiz::graph;

//
// mp3decoder
//
graph::mp3decoder::mp3decoder () : graph::graph ("mp3decodergraph")
{
}

graph::ops *graph::mp3decoder::do_init ()
{
  omx_comp_name_lst_t comp_list;
  comp_list.push_back ("OMX.Aratelia.file_reader.binary");
  comp_list.push_back ("OMX.Aratelia.audio_decoder.mp3");
  comp_list.push_back ("OMX.Aratelia.audio_renderer_nb.pcm");

  omx_comp_role_lst_t role_list;
  role_list.push_back ("audio_reader.binary");
  role_list.push_back ("audio_decoder.mp3");
  role_list.push_back ("audio_renderer.pcm");

  return new mp3decops (this, comp_list, role_list);
}

//
// mp3decops
//
graph::mp3decops::mp3decops (graph *p_graph,
                             const omx_comp_name_lst_t &comp_lst,
                             const omx_comp_role_lst_t &role_lst)
  : tiz::graph::ops (p_graph, comp_lst, role_lst),
    need_port_settings_changed_evt_ (false)
{
}

void graph::mp3decops::do_probe ()
{
  TIZ_LOG (TIZ_PRIORITY_TRACE, "current_file_index_ [%d]...",
           current_file_index_);
  assert (current_file_index_ < file_list_.size ());
  G_OPS_BAIL_IF_ERROR (probe_uri (current_file_index_), "Unable to probe uri.");
  G_OPS_BAIL_IF_ERROR (
      tiz::graph::util::set_mp3_type (
          handles_[1], 0,
          boost::bind (&tiz::probe::get_mp3_codec_info, probe_ptr_, _1),
          need_port_settings_changed_evt_),
      "Unable to set OMX_IndexParamAudioMp3");
}

bool graph::mp3decops::is_port_settings_evt_required () const
{
  return need_port_settings_changed_evt_;
}

bool graph::mp3decops::is_disabled_evt_required () const
{
  return false;
}

void graph::mp3decops::do_configure ()
{
  G_OPS_BAIL_IF_ERROR (
      util::set_content_uri (handles_[0], probe_ptr_->get_uri ()),
      "Unable to set OMX_IndexParamContentURI");
  G_OPS_BAIL_IF_ERROR (
      tiz::graph::util::set_pcm_mode (
          handles_[2], 0,
          boost::bind (&tiz::probe::get_pcm_codec_info, probe_ptr_, _1)),
      "Unable to set OMX_IndexParamAudioPcm");
}

OMX_ERRORTYPE
graph::mp3decops::probe_uri (const int uri_index, const bool quiet)
{
  assert (uri_index < file_list_.size ());

  const std::string &uri = file_list_[uri_index];

  if (!uri.empty ())
  {
    // Probe a new uri
    probe_ptr_.reset ();
    bool quiet_probing = true;
    probe_ptr_ = boost::make_shared<tiz::probe>(uri, quiet_probing);
    if (probe_ptr_->get_omx_domain () != OMX_PortDomainAudio
        || probe_ptr_->get_audio_coding_type () != OMX_AUDIO_CodingMP3)
    {
      return OMX_ErrorContentURIError;
    }
    if (!quiet)
    {
      tiz::graph::util::dump_graph_info ("mp3", "decode", uri);
      probe_ptr_->dump_stream_metadata ();
      probe_ptr_->dump_mp3_and_pcm_info ();
    }
  }
  return OMX_ErrorNone;
}
