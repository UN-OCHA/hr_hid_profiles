(function($) {
Drupal.behaviors.hidProfilesContacts = {
  attach: function (context, settings) {
    Contact = Backbone.Model.extend({
      url: function() {
        return window.location.protocol + '//' + window.location.host + '/hid/proxy?api_path=v0/contact/view&_id='+this.get('_id');
      },
      parse: function(response) {
        if (response.contacts != undefined) {
          return response.contacts[0];
        }
        else {
          return response;
        }
      },
      getMainOrganizationName: function() {
        var organizations = this.get('organization');
        if (organizations.length > 0) {
          return organizations[0].name;
        }
      },
      getLocationName: function() {
        var address = this.get('address');
        if (address.length > 0) {
          return address[0].locality;
        }
      },

      getBundles: function() {
        var bundles = this.get('bundle');
        if (bundles.length > 0) {
          return bundles.join(", ");
        }
      },
    });

    ContactList = Backbone.Collection.extend({
        model: Contact,
        url: function() {
          var index = window.location.hash.indexOf('?');
          var url = window.location.protocol + '//' + window.location.host + '/hid/proxy?api_path=v0/contact/view&locationId=hrinfo:' + settings.hid_profiles.operation_id + '&type=local&limit=' + this.limit + '&skip=' + this.skip;
          if (index != -1) {
            var params = window.location.hash.substr(index + 1);
            url += '&' + params;
          }
          return url;
        },
        parse: function(response) {
           return response.contacts;
        },
        limit: 5,
        skip: 0,
    });

    ContactView = Backbone.View.extend({
      router: null,

      show: function() {
        this.$el.show();
      },

      hide: function() {
        this.$el.hide();
      },

      clear: function() {
        this.$el.empty();
      },

      loading: function() {
        this.hide();
        $('#loading').show();
      },

      finishedLoading: function() {
        $('#loading').hide();
        this.show();
      },

    });
    
    ContactTableView = ContactView.extend({
        
        numItems: 10,
        currentPage: 1,
        
        initialize: function() {
            this.contactsList = new ContactList;
            this.contactsList.limit = this.numItems;
        },

        loadResults: function() {
          var that = this;
          this.contactsList.fetch({ 
            success: function (contacts) {
              var template = _.template($('#contacts_list_table_row').html());
              $('#contacts-list-table tbody').append(template({contacts: contacts.models}));
              that.finishedLoading();
            },
          });
        },

        events: {
          'change #protectedRoles': 'filterByProtectedRoles',
          'change #bundles': 'filterByBundles',
          'click #search-button': 'search',
          'keyup #search': 'search',
        },

        page: function(page) {
          this.loading();
          this.currentPage = page;
          this.clear();
          this.contactsList.skip = this.numItems * (page - 1);
          this.loadResults();
        },

        clear: function() {
          $('#contacts-list-table tbody').empty();
        },

        show: function() {
          $('#contacts-list').show();
        },

        hide: function() {
          $('#contacts-list').hide();
        },

        filterByProtectedRoles: function(event) {
          this.router.navigate('table/1?protectedRoles=' + $('#protectedRoles').val(), {trigger: true});
        },

        filterByBundles: function(event) {
          this.router.navigate('table/1?bundle=' + $('#bundles').val(), {trigger: true});
        },

        search: function(event) {
          if (event.type == 'keyup' && event.keyCode == 13 || event.type == 'click') {
            this.router.navigate('table/1?text='+$('#search').val(), {trigger: true});
          }
        },

    });

    ContactItemView = ContactView.extend({
      render: function (model) {
        var template = _.template($('#contacts_view').html());
        this.$el.html(template({contact: model}));
      },
    });

    ContactRouter = Backbone.Router.extend({
      routes: {
        "contact/:id" : "contact",
        "table/:page" : "table",
        "*actions": "defaultRoute",
      },

      tableView: new ContactTableView({el: 'body'}),
      contactView: new ContactItemView({el: '#contacts-view'}),

      initialize: function() {
        this.tableView.router = this;
        this.contactView.router = this;
      },

      defaultRoute: function (actions) {
        this.navigate('table/1', {trigger: true});
      },

      table: function(page) {
        this.contactView.hide();
        this.tableView.show();
        var nextPage = parseInt(page) + 1;
        var previousPage = parseInt(page) - 1;
        $('#next').attr('href', '#table/' + nextPage);
        if (previousPage > 0) {
          $('#previous').attr('href', '#table/' + previousPage);
        }
        this.tableView.page(page);
      },

      contact: function(id) {
        this.contactView.loading();
        var that = this;
        this.tableView.hide();
        this.contactView.clear();
        var contact = new Contact({_id: id});
        contact.fetch({
          success: function(contact) {
            that.contactView.clear();
            that.contactView.render(contact);
            that.contactView.finishedLoading();
          },
        });
      },
    });

    var contact_router = new ContactRouter;

    Backbone.history.start();

  }
}
})(jQuery);
