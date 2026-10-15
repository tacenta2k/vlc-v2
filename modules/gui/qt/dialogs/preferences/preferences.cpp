/*****************************************************************************
 * preferences.cpp : Preferences
 *****************************************************************************
 * Copyright (C) 2006-2007 the VideoLAN team
 *
 * Authors: Clément Stenac <zorglub@videolan.org>
 *          Jean-Baptiste Kempf <jb@videolan.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/

#ifdef HAVE_CONFIG_H
# include "config.h"
#endif

#include "dialogs/preferences/preferences.hpp"

#include "expert_view.hpp"
#include "dialogs/preferences/complete_preferences.hpp"
#include "dialogs/preferences/simple_preferences.hpp"
#include "dialogs/dialogs/dialogmodel.hpp"
#include "dialogs/dialogs_provider.hpp"
#include "widgets/native/searchlineedit.hpp"
#include "widgets/native/qvlcframe.hpp"
#include "maininterface/mainctx.hpp"

#include <QHBoxLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QPushButton>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QStackedWidget>
#include <QSplitter>
#include <QShortcut>
#include <QFrame>

#include <vlc_configuration.h>
#include <vlc_modules.h>

PrefsDialog::PrefsDialog( QWindow *parent, qt_intf_t *_p_intf )
            : QVLCDialog( parent, _p_intf )
{
    setObjectName( "vlcSettingsDialog" );
    setWindowTitle( qtr( "Settings" ) );
    setWindowRole( "vlc-preferences" );
    setMinimumSize( QSize( 820, 620 ) );
    setWindowModality( Qt::WindowModal );

    /* Whether we want it or not, we need to destroy on close to get
       consistency when reset */
    setAttribute( Qt::WA_DeleteOnClose );

    p_list = NULL;

    QGridLayout *main_layout = new QGridLayout( this );
    main_layout->setObjectName( "settingsRootLayout" );
    setLayout( main_layout );

    /* View (panel) selection */
    types = new QGroupBox( qtr("Show settings") );
    types->setObjectName( "settingsModeSelector" );
    types->setAlignment( Qt::AlignHCenter );
    QHBoxLayout *types_l = new QHBoxLayout;
    types_l->setSpacing( 3 );
    types_l->setContentsMargins(3, 3, 3, 3);
    simple = new QRadioButton( qtr( "Simple" ), types );
    simple->setToolTip( qtr( "Switch to simple preferences view" ) );
    all = new QRadioButton( qtr("All"), types );
    all->setToolTip( qtr( "Switch to full preferences view" ) );
    expert = new QRadioButton( qtr("Expert"), types );
    expert->setToolTip( qtr( "Switch to expert preferences view" ) );
    types_l->addWidget( simple );
    types_l->addWidget( all );
    types_l->addWidget( expert );
    types->setLayout( types_l );
    simple->setChecked( true );

    /* Buttons */
    QDialogButtonBox *buttonsBox = new QDialogButtonBox();
    buttonsBox->setObjectName( "settingsButtonBox" );
    QPushButton *save = new QPushButton( qtr( "&Save" ) );
    save->setObjectName( "settingsSaveButton" );
    save->setToolTip( qtr( "Save and close the dialog" ) );
    QPushButton *cancel = new QPushButton( qtr( "&Cancel" ) );
    cancel->setObjectName( "settingsCancelButton" );
    QPushButton *reset = new QPushButton( qtr( "&Reset Preferences" ) );
    reset->setObjectName( "settingsResetButton" );

    buttonsBox->addButton( save, QDialogButtonBox::AcceptRole );
    buttonsBox->addButton( cancel, QDialogButtonBox::RejectRole );
    buttonsBox->addButton( reset, QDialogButtonBox::ResetRole );

    /* View (panel) stack */
    stack = new QStackedWidget();
    stack->setObjectName( "settingsViewStack" );

    /* Simple view (panel) */
    simple_split_widget = new QWidget();
    simple_split_widget->setObjectName( "settingsSimpleSplit" );
    simple_split_widget->setLayout( new QHBoxLayout );

    simple_tree_panel = new QWidget;
    simple_tree_panel->setObjectName( "settingsSidebar" );
    simple_tree_panel->setLayout( new QVBoxLayout );
    simple_tree_panel->setMinimumWidth( 205 );
    simple_tree_panel->setMaximumWidth( 225 );
    simple_tree = NULL;
    simple_panels_stack = new QStackedWidget;
    simple_panels_stack->setObjectName( "settingsContent" );
    for( int i = 0; i < SPrefsMax ; i++ )
        simple_panels[i] = NULL;

    simple_split_widget->layout()->addWidget( simple_tree_panel );
    simple_split_widget->layout()->addWidget( simple_panels_stack, 1 );

    simple_tree_panel->layout()->setContentsMargins( 10, 10, 10, 10 );
    simple_tree_panel->layout()->setSpacing( 8 );
    simple_panels_stack->setContentsMargins( 0, 0, 0, 0 );
    simple_split_widget->layout()->setContentsMargins( 0, 0, 0, 0 );
    simple_split_widget->layout()->setSpacing( 12 );

    stack->insertWidget( SIMPLE, simple_split_widget );

    /* Advanced view (panel) */
    advanced_split_widget = new QSplitter();

    advanced_tree_panel = new QWidget;
    advanced_tree_panel->setLayout( new QVBoxLayout );
    tree_filter = NULL;
    current_filter = NULL;
    advanced_tree = NULL;
    advanced_panels_stack = new QStackedWidget;

    advanced_split_widget->addWidget( advanced_tree_panel );
    advanced_split_widget->addWidget( advanced_panels_stack );

    advanced_split_widget->setSizes(QList<int>() << 300 << 550);
    advanced_tree_panel->sizePolicy().setHorizontalStretch(1);
    advanced_panels_stack->sizePolicy().setHorizontalStretch(2);

    stack->insertWidget( ADVANCED, advanced_split_widget );

    /* Expert view (panel) */
    expert_widget = new QWidget;
    expert_widget_layout = new QGridLayout;
    expert_widget->setLayout( expert_widget_layout );

    expert_table_filter = nullptr;
    expert_table = nullptr;

    expert_text = new QLabel;
    expert_longtext = new QLabel;

    expert_text->setWordWrap(true);
    expert_longtext->setWordWrap(true);

    QFont textFont = QApplication::font();
    textFont.setPointSize( textFont.pointSize() + 2 );
    textFont.setUnderline( true );
    expert_text->setFont( textFont );

    expert_widget_layout->addWidget( expert_text, 2, 0, 1, 2 );
    expert_widget_layout->addWidget( expert_longtext, 3, 0, 1, 2 );

    stack->insertWidget( EXPERT, expert_widget );

    /* Layout  */
    main_layout->addWidget( stack, 0, 0, 3, 3 );
    main_layout->addWidget( types, 3, 0, 2, 1 );
    main_layout->addWidget( buttonsBox, 4, 2, 1 ,1 );
    main_layout->setRowStretch( 2, 4 );
    main_layout->setContentsMargins( 12, 12, 12, 12 );
    main_layout->setHorizontalSpacing( 12 );
    main_layout->setVerticalSpacing( 8 );

    setStyleSheet( QStringLiteral( R"VLCQSS(
        QDialog#vlcSettingsDialog {
            background: #0E0E0E;
            color: #969696;
        }
        QWidget#settingsSimpleSplit,
        QStackedWidget#settingsViewStack,
        QWidget#settingsContent {
            background: #0E0E0E;
            color: #969696;
            border: 0;
        }
        QWidget#settingsSidebar {
            background: #191919;
            border: 0;
            border-radius: 8px;
        }
        QWidget#settingsPage {
            background: #0E0E0E;
            color: #969696;
        }
        QLabel {
            color: #969696;
            background: transparent;
        }
        QLabel#settingsPageTitle {
            color: #E5E5E5;
            font-size: 18px;
            font-weight: 500;
            padding-bottom: 8px;
        }
        QFrame#settingsHeaderDivider {
            color: #242424;
            background: #242424;
            border: 0;
            max-height: 1px;
        }
        QLabel#settingsSectionTitle {
            color: #C5C5C5;
            font-size: 14px;
            font-weight: 600;
            padding-top: 8px;
            padding-bottom: 3px;
        }
        QGroupBox {
            color: #C5C5C5;
            background: transparent;
            border: 1px solid #262626;
            border-radius: 7px;
            margin-top: 12px;
            padding: 12px 8px 8px 8px;
            font-weight: 600;
        }
        QGroupBox::title {
            subcontrol-origin: margin;
            left: 12px;
            padding: 0 5px;
            color: #C5C5C5;
        }
        QLineEdit, QComboBox, QSpinBox, QDoubleSpinBox, QPlainTextEdit, QTextEdit {
            background: #171717;
            color: #D0D0D0;
            border: 1px solid #292929;
            border-radius: 5px;
            min-height: 28px;
            padding: 2px 8px;
            selection-background-color: #D37918;
            selection-color: #101010;
        }
        QLineEdit:focus, QComboBox:focus, QSpinBox:focus, QDoubleSpinBox:focus,
        QPlainTextEdit:focus, QTextEdit:focus {
            border: 1px solid #D37918;
        }
        QComboBox::drop-down {
            border: 0;
            width: 24px;
        }
        QComboBox QAbstractItemView {
            background: #191919;
            color: #D0D0D0;
            selection-background-color: #2A2A2A;
            selection-color: #F0F0F0;
            border: 1px solid #303030;
            outline: 0;
        }
        QPushButton, QToolButton {
            background: #1B1B1B;
            color: #B8B8B8;
            border: 1px solid #292929;
            border-radius: 5px;
            padding: 7px 12px;
            min-height: 26px;
        }
        QPushButton:hover, QToolButton:hover {
            background: #242424;
            color: #E5E5E5;
            border-color: #393939;
        }
        QPushButton:pressed, QToolButton:pressed {
            background: #2A2A2A;
        }
        QPushButton:disabled, QToolButton:disabled {
            color: #5F5F5F;
            background: #151515;
            border-color: #202020;
        }
        QPushButton#settingsSaveButton {
            background: #D37918;
            color: #111111;
            border: 1px solid #D37918;
            font-weight: 600;
            padding-left: 18px;
            padding-right: 18px;
        }
        QPushButton#settingsSaveButton:hover {
            background: #E38A28;
            border-color: #E38A28;
        }
        QPushButton#settingsCancelButton {
            background: #1B1B1B;
            color: #CECECE;
        }
        QPushButton#settingsResetButton {
            background: transparent;
            color: #969696;
            border-color: transparent;
        }
        QToolButton#settingsCategoryButton {
            text-align: left;
            padding: 7px 10px;
            min-height: 24px;
            border: 1px solid transparent;
            border-radius: 5px;
            background: transparent;
        }
        QToolButton#settingsCategoryButton:hover {
            background: #222222;
            color: #E5E5E5;
        }
        QToolButton#settingsCategoryButton:checked {
            background: #242424;
            color: #E5E5E5;
            border-left: 3px solid #D37918;
            padding-left: 8px;
        }
        QLineEdit#settingsSearchField {
            background: #101010;
            color: #D0D0D0;
            border: 1px solid #262626;
            border-radius: 6px;
            padding-left: 10px;
            padding-right: 8px;
            min-height: 30px;
        }
        QLabel#settingsSectionLabel {
            color: #626262;
            font-size: 10px;
            font-weight: 600;
            padding: 10px 8px 3px 8px;
        }
        QCheckBox, QRadioButton {
            color: #969696;
            spacing: 8px;
            background: transparent;
        }
        QCheckBox::indicator, QRadioButton::indicator {
            width: 16px;
            height: 16px;
        }
        QCheckBox::indicator:unchecked, QRadioButton::indicator:unchecked {
            background: #171717;
            border: 1px solid #494949;
            border-radius: 3px;
        }
        QCheckBox::indicator:checked, QRadioButton::indicator:checked {
            background: #D37918;
            border: 1px solid #D37918;
            border-radius: 3px;
        }
        QRadioButton::indicator:unchecked, QRadioButton::indicator:checked {
            border-radius: 8px;
        }
        QSlider::groove:horizontal {
            background: #282828;
            height: 4px;
            border-radius: 2px;
        }
        QSlider::sub-page:horizontal {
            background: #D37918;
            border-radius: 2px;
        }
        QSlider::handle:horizontal {
            background: #D37918;
            border: 0;
            width: 12px;
            height: 12px;
            margin: -5px 0;
            border-radius: 6px;
        }
        QScrollBar:vertical {
            background: transparent;
            width: 8px;
            margin: 2px;
        }
        QScrollBar::handle:vertical {
            background: #3C3C3C;
            border-radius: 4px;
            min-height: 22px;
        }
        QScrollBar::handle:vertical:hover {
            background: #555555;
        }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical,
        QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {
            background: transparent;
            border: 0;
            height: 0;
        }
        QTreeWidget, QTreeView, QTableView, QTableWidget {
            background: #111111;
            color: #A0A0A0;
            alternate-background-color: #171717;
            border: 1px solid #262626;
            selection-background-color: #282828;
            selection-color: #F0F0F0;
            gridline-color: #242424;
            outline: 0;
        }
        QHeaderView::section {
            background: #191919;
            color: #8F8F8F;
            border: 0;
            border-bottom: 1px solid #282828;
            padding: 7px;
        }
        QDialogButtonBox {
            border: 0;
            background: transparent;
        }
        QGroupBox#settingsModeSelector {
            border: 0;
            margin: 0;
            padding: 0;
            font-weight: normal;
        }
        QGroupBox#settingsModeSelector::title {
            color: #6B6B6B;
            left: 0;
            padding: 0;
        }
    )VLCQSS") );

    switch( var_InheritInteger( p_intf, "qt-initial-prefs-view" ) )
    {
        case 2:  setExpert();   break;
        case 1:  setAdvanced(); break;
        default: setSimple();   break;
    }

    BUTTONACT( save, &PrefsDialog::save );
    BUTTONACT( cancel, &PrefsDialog::cancel );
    BUTTONACT( reset, &PrefsDialog::reset );

    BUTTONACT( simple, &PrefsDialog::setSimple );
    BUTTONACT( all, &PrefsDialog::setAdvanced );
    BUTTONACT( expert, &PrefsDialog::setExpert );

    QVLCTools::restoreWidgetPosition( p_intf, "Preferences", this, QSize( 1048, 865 ) );
}

PrefsDialog::~PrefsDialog()
{
    module_list_free( p_list );
}

void PrefsDialog::setExpert()
{
    /* Lazy creation */
    if( !expert_table )
    {
        if ( !p_list )
            p_list = module_list_get( &count );

        expert_table_filter = new SearchLineEdit( expert_widget );
        expert_table_filter->setMinimumHeight( 26 );
        expert_table_filter_modified = new QCheckBox( qtr( "Modified only" ), expert_widget );

        QShortcut *search = new QShortcut( QKeySequence( QKeySequence::Find ), expert_table_filter );

        ExpertPrefsTableModel *table_model = new ExpertPrefsTableModel( p_list, count, this );
        expert_table = new ExpertPrefsTable( expert_widget );
        expert_table->setModel( table_model );
        expert_table->resizeColumnToContents( ExpertPrefsTableModel::NameField );

        expert_widget_layout->addWidget( expert_table_filter, 0, 0 );
        expert_widget_layout->addWidget( expert_table_filter_modified, 0, 1 );
        expert_widget_layout->addWidget( expert_table, 1, 0, 1, 2 );

        connect( expert_table->selectionModel(), &QItemSelectionModel::currentChanged,
                 this, &PrefsDialog::changeExpertDesc );
        connect( expert_table_filter, &SearchLineEdit::textChanged,
                 this, &PrefsDialog::expertTableFilterChanged );
        connect( expert_table_filter_modified, &QCheckBox::toggled,
                 this, &PrefsDialog::expertTableFilterModifiedToggled );
        connect( search, &QShortcut::activated,
                 expert_table_filter, QOverload<>::of(&SearchLineEdit::setFocus) );

        /* Set initial selection */
        expert_table->setCurrentIndex(
                expert_table->model()->index( 0, 0, QModelIndex() ) );
    }

    expert->setChecked( true );
    stack->setCurrentIndex( EXPERT );
    setWindowTitle( qtr( "Expert Settings" ) );
}

void PrefsDialog::setAdvanced()
{
    /* Lazy creation */
    if( !advanced_tree )
    {
        if ( !p_list )
            p_list = module_list_get( &count );

        advanced_tree = new PrefsTree( p_intf, advanced_tree_panel, p_list, count );

        tree_filter = new SearchLineEdit( advanced_tree_panel );
        tree_filter->setMinimumHeight( 26 );

        current_filter = new QCheckBox( qtr("Only show current") );
        current_filter->setToolTip(
                    qtr("Only show modules related to current playback") );

        QShortcut *search = new QShortcut( QKeySequence( QKeySequence::Find ), tree_filter );

        advanced_tree_panel->layout()->addWidget( tree_filter );
        advanced_tree_panel->layout()->addWidget( current_filter );
        advanced_tree_panel->layout()->addWidget( advanced_tree );
        advanced_tree_panel->setSizePolicy( QSizePolicy::Maximum, QSizePolicy::Preferred );

        connect( advanced_tree, &PrefsTree::currentItemChanged, this, &PrefsDialog::changeAdvPanel );
        connect( tree_filter, &SearchLineEdit::textChanged, this, &PrefsDialog::advancedTreeFilterChanged );
        connect( current_filter, &QtCheckboxChanged, this, &PrefsDialog::onlyLoadedToggled );
        connect( search, &QShortcut::activated, tree_filter, QOverload<>::of(&SearchLineEdit::setFocus) );

        /* Set initial selection */
        advanced_tree->setCurrentIndex(
                advanced_tree->model()->index( 0, 0, QModelIndex() ) );
    }

    all->setChecked( true );
    stack->setCurrentIndex( ADVANCED );
    setWindowTitle( qtr( "Advanced Settings" ) );
}

void PrefsDialog::setSimple()
{
    /* If no simple_tree, create one, connect it */
    if( !simple_tree )
    {
        simple_tree = new SPrefsCatList( p_intf, simple_tree_panel );
        connect( simple_tree, &SPrefsCatList::currentItemChanged,
                 this, &PrefsDialog::changeSimplePanel );
        simple_tree_panel->layout()->addWidget( simple_tree );
        simple_tree_panel->setSizePolicy( QSizePolicy::Fixed, QSizePolicy::Preferred );
    }

    if( ! simple_panels[SPrefsDefaultCat] )
        changeSimplePanel( SPrefsDefaultCat );

    simple->setChecked( true );
    stack->setCurrentIndex( SIMPLE );
    setWindowTitle( qtr( "Settings" ) );
}

/* Switching from on simple panel to another */
void PrefsDialog::changeSimplePanel( int number )
{
    if( ! simple_panels[number] )
    {
        SPrefsPanel *insert = new SPrefsPanel( p_intf, simple_panels_stack, number ) ;
        connect( insert, &SPrefsPanel::advancedSettingsRequested,
                 this, &PrefsDialog::setAdvanced );
        simple_panels_stack->insertWidget( number, insert );
        simple_panels[number] = insert;
    }
    simple_panels_stack->setCurrentWidget( simple_panels[number] );
}

/* Changing from one Advanced Panel to another */
void PrefsDialog::changeAdvPanel( QTreeWidgetItem *item )
{
    if( item == NULL ) return;
    PrefsTreeItem *node = static_cast<PrefsTreeItem *>( item );

    if( !node->panel )
    {
        node->panel = new AdvPrefsPanel( p_intf, advanced_panels_stack, node );
        advanced_panels_stack->addWidget( node->panel );
    }
    advanced_panels_stack->setCurrentWidget( node->panel );
}

/* Changing from one Expert item description to another */
void PrefsDialog::changeExpertDesc( const QModelIndex &current, const QModelIndex &previous )
{
    Q_UNUSED( previous );
    if( !current.isValid() )
       return;
    ExpertPrefsTableItem *item = expert_table->myModel()->itemAt( current );

    expert_text->setText( item->getTitle() );
    expert_longtext->setText( item->getDescription() );
}

/* Actual apply and save for the preferences */
void PrefsDialog::save()
{
    if( simple->isChecked() && simple_tree->isVisible() )
    {
        msg_Dbg( p_intf, "Saving the simple preferences" );
        for( int i = 0 ; i< SPrefsMax; i++ ){
            if( simple_panels_stack->widget(i) )
                qobject_cast<SPrefsPanel *>(simple_panels_stack->widget(i))->apply();
        }
    }
    else if( all->isChecked() && advanced_tree->isVisible() )
    {
        msg_Dbg( p_intf, "Saving the advanced preferences" );
        advanced_tree->applyAll();
    }
    else if( expert->isChecked() && expert_table->isVisible() )
    {
        msg_Dbg( p_intf, "Saving the expert preferences" );
        expert_table->applyAll();
    }

    /* Save to file */
    if( config_SaveConfigFile( p_intf ) != 0 )
    {
        DialogErrorModel::getInstance(p_intf)->pushError(
            qtr( "Cannot save Configuration" ),
            qtr("Preferences file could not be saved")
        );
        if (p_intf->b_isDialogProvider)
        {
            //at this point we are closing the preference dialog which is modal,
            //we need to postpone the dialog raising up
            QMetaObject::invokeMethod(DialogsProvider::getInstance(), [](){
                DialogsProvider::getInstance()->messagesDialog(1);
            }, Qt::QueuedConnection);
        }
    }

    if( p_intf->p_mi )
        p_intf->p_mi->reloadPrefs();
    accept();

    // Don't save widget position if reset is pending
    if( !p_intf->preferencesResetPending )
        QVLCTools::saveWidgetPosition( p_intf, "Preferences", this );

    if( p_intf->preferencesResetPending )
    {
        int restart = QMessageBox::question(
                          this,
                          qtr( "Restart Required" ),
                          qtr( "VLC needs to be restarted for the reset to take full effect.\n\nRestart now?" ),
                          QMessageBox::Yes | QMessageBox::No,
                          QMessageBox::Yes);

        if( restart == QMessageBox::Yes )
        {
            // Quit VLC
            emit p_intf->p_mi->askToQuit();
        }
        else
        {
            // User chose not to restart - clear the flag
            p_intf->preferencesResetPending = false;
        }
    }

}

/* Clean the preferences, dunno if it does something really */
void PrefsDialog::cancel()
{
    bool wasResetting = p_intf->preferencesResetPending;

    // Clear the reset flag
    p_intf->preferencesResetPending = false;

    if( !wasResetting )
        QVLCTools::saveWidgetPosition( p_intf, "Preferences", this );
    reject();
}

/* Reset all the preferences, when you click the button */
void PrefsDialog::reset()
{
    int ret = QMessageBox::question(
                 this,
                 qtr( "Reset Preferences" ),
                 qtr( "Are you sure you want to reset your VLC media player preferences?" ),
                 QMessageBox::Ok | QMessageBox::Cancel,
                 QMessageBox::Ok);

    if( ret == QMessageBox::Ok )
    {
        config_ResetAll();
        config_SaveConfigFile( p_intf );
        getSettings()->clear();
        p_intf->preferencesResetPending = true;
        p_intf->p_mi->reloadPrefs();
        p_intf->p_mi->reloadFromSettings();

#ifdef _WIN32
        simple_panels[0]->cleanLang();
#endif

    }
}

void PrefsDialog::expertTableFilterModifiedToggled( bool checked )
{
    expert_table->filter( expert_table_filter->text(), checked );
}

void PrefsDialog::expertTableFilterChanged( const QString & text )
{
    expert_table->filter( text, expert_table_filter_modified->isChecked() );
}

void PrefsDialog::advancedTreeFilterChanged( const QString & text )
{
    advanced_tree->filter( text );
}

void PrefsDialog::onlyLoadedToggled()
{
    advanced_tree->setLoadedOnly( current_filter->isChecked() );
}
