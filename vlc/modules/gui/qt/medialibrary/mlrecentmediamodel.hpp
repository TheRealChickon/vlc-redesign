/*****************************************************************************
 * Copyright (C) 2020 VLC authors and VideoLAN
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * ( at your option ) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifndef ML_RECENT_MEDIA_MODEL_H
#define ML_RECENT_MEDIA_MODEL_H

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "mlmediamodel.hpp"

class MLRecentMediaModel : public MLMediaModel
{
    Q_OBJECT
    Q_PROPERTY(bool audioOnly READ audioOnly WRITE setAudioOnly NOTIFY audioOnlyChanged FINAL)
public:
    bool audioOnly() const { return m_audioOnly; }
    void setAudioOnly(bool value) {
        if (m_audioOnly == value) return;
        m_audioOnly = value;
        emit audioOnlyChanged();
        emit resetRequested();
    }
signals:
    void audioOnlyChanged();
public:
    enum Roles
    {
        RECENT_MEDIA_URL = MLMediaModel::MEDIA_ROLES_COUNT,
    };

    explicit MLRecentMediaModel( QObject* parent = nullptr );
    virtual ~MLRecentMediaModel() = default;

    Q_INVOKABLE void clearHistory();

protected:
    QHash<int, QByteArray> roleNames() const override;

    QVariant itemRoleData(const MLItem *item, int role) const override;

    std::unique_ptr<MLListCacheLoader> createMLLoader() const override;

private:
    bool m_audioOnly = false;
    void onVlcMlEvent( const MLEvent &event ) override;

    struct Loader : public MLListCacheLoader::MLOp
    {
        explicit Loader(const MLRecentMediaModel &model)
            : MLOp(model), audioOnly(model.m_audioOnly) {}
        bool audioOnly;
        size_t count(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
        std::vector<std::unique_ptr<MLItem>> load(vlc_medialibrary_t* ml, const vlc_ml_query_params_t* queryParams) const override;
        std::unique_ptr<MLItem> loadItemById(vlc_medialibrary_t* ml, MLItemId itemId) const override;
    };
};

#endif // ML_RECENT_MEDIA_MODEL_H
