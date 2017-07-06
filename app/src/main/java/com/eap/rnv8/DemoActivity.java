package com.eap.rnv8;

import java.util.Arrays;
import java.util.List;

import com.facebook.infer.annotation.Assertions;
import com.facebook.react.ReactActivity;
import com.facebook.react.ReactActivityDelegate;
import com.facebook.react.ReactInstanceManager;
import com.facebook.react.ReactInstanceManagerBuilder;
import com.facebook.react.ReactNativeHost;
import com.facebook.react.ReactPackage;
import com.facebook.react.bridge.queue.MessageQueueThread;
import com.facebook.react.bridge.queue.MessageQueueThreadImpl;
import com.facebook.react.bridge.queue.MessageQueueThreadSpec;
import com.facebook.react.bridge.queue.QueueThreadExceptionHandler;
import com.facebook.react.common.LifecycleState;
import com.facebook.react.shell.MainReactPackage;

import android.os.Bundle;

public class DemoActivity extends ReactActivity {
    private static MessageQueueThread sMessageQueueThread = null;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        if(null == sMessageQueueThread) {
            sMessageQueueThread = MessageQueueThreadImpl.create(
                    MessageQueueThreadSpec.newBackgroundThreadSpec("v8.Engine"),
                    new QueueThreadExceptionHandler() {

                        @Override
                        public void handleException(Exception e) {

                        }
                    });
        }
        super.onCreate(savedInstanceState);
    }

    /**
     * Returns the name of the main component registered from JavaScript.
     * This is used to schedule rendering of the component.
     */
    @Override
    protected String getMainComponentName() {
        return "demo";
    }

    @Override
    protected ReactActivityDelegate createReactActivityDelegate() {
        return new ReactActivityDelegate(this, getMainComponentName()) {
            ReactNativeHost mReactNativeHost = null;

            protected ReactNativeHost getReactNativeHost() {
                if (null == mReactNativeHost) {
                    mReactNativeHost = new ReactNativeHost(getApplication()) {
                        @Override
                        public boolean getUseDeveloperSupport() {
                            return true;
                        }

                        @Override
                        protected List<ReactPackage> getPackages() {
                            return Arrays.<ReactPackage>asList(
                                    new MainReactPackage()
                            );
                        }

                        protected ReactInstanceManager createReactInstanceManager() {
                            ReactInstanceManagerBuilder builder = ReactInstanceManager.builder()
                                    .setApplication(getApplication())
                                    .setJSMainModuleName(getJSMainModuleName())
                                    .setUseDeveloperSupport(getUseDeveloperSupport())
                                    .setRedBoxHandler(getRedBoxHandler())
                                    .setUIImplementationProvider(getUIImplementationProvider())
                                    .setCustomJSMessageQueueThread(sMessageQueueThread)
                                    .setInitialLifecycleState(LifecycleState.BEFORE_CREATE);

                            for (ReactPackage reactPackage : getPackages()) {
                                builder.addPackage(reactPackage);
                            }

                            String jsBundleFile = getJSBundleFile();
                            if (jsBundleFile != null) {
                                builder.setJSBundleFile(jsBundleFile);
                            } else {
                                builder.setBundleAssetName(Assertions.assertNotNull(getBundleAssetName()));
                            }
                            return builder.build();
                        }
                    };
                }
                return mReactNativeHost;
            }
        };
    }
}
