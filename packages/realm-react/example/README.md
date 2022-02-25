# Realm React Example App

This app implements a simple todo list, using Realm for persistence and the Realm React hooks for React integration. It supports sync, allowing users to login and sync their todo lists across multiple devices.

## Running the app

The app is a standard React Native project, but requires bootstrapping with Lerna first:

1. Bootstrap the project: `npx lerna bootstrap --scope="@realm/react-example" --include-dependencies`
2. `npm run ios` or `npm run android` to run the app

## Enabling sync

### Setting up a Realm app

To enable sync, first you need to set up a new free Realm app:

1. Go to https://www.mongodb.com/cloud/atlas/register and register a new account and fill out the initial questionaire
2. On the "deploy a cloud database" screen, click "I'll do this later" (in the bottom left)
3. From the Atlas homepage, click on the Realm tab at the top
4. From the "Welcome to MongoDB Realm" wizard, select "Build your own app" and click "Next"
5. Accept the default values for data source, application and deployment model by clicking "Create Realm Application"
6. Close the "Welcome to your Application Guides" popup
7. Choose "Sync" from the left hand menu
8. Press "Start Syncing"
9. In the "Already have data in Atlas?" popup, click "No thanks, continue to Sync"
